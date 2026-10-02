#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <unordered_map>

// Emits one entry point as MSL 2.0 (Metal), compiled by Metal when the pipeline is created. Shaped like the HLSL
// emitter's output:
//
// - Every value is a variable assigned once, v0, v1... Pointers are kept as the lvalue text they stand for, like
//   l0.m1.e[v2]: in shaders every pointer is a variable followed by access steps (Docs/IR.md).
// - Arrays are wrapped in structs, struct A0 { float2 e[3]; }, so they can be assigned and copied like everything else,
//   including from constant memory.
// - Pointer parameters, which the IR uses for arrays and structs, are thread references.
// - Interface globals are fields of the entry function's in and out structs, with their attributes, except inputs
//   Metal only takes as parameters, like [[vertex_id]]. The entry function is main0: Metal doesn't allow main.
// - Control flow is printed from the IR's structure: an if per selection_merge. Only blocks reachable from the entry
//   block are printed.
// - Only generated names: user identifiers never reach the output.
//
// Not safe for untrusted shaders yet: integer division by zero, shifts past the width, signed overflow and float to int
// conversions out of range are undefined behavior in MSL, which LLVM exploits. Guarding them is Docs/Plan/Shaders.md,
// 11.2.

namespace EVA::Script
{

namespace
{

PrimitiveKind ScalarKind(Type* type)
{
	if (type->type_kind == TypeKind::MATRIX)
		return ((MatrixType*)type)->element->primitive_kind;
	return ComponentType(type)->primitive_kind;
}

const char* ScalarName(PrimitiveKind kind)
{
	switch (kind)
	{
	case PrimitiveKind::VOID: return "void";
	case PrimitiveKind::BOOL: return "bool";
	case PrimitiveKind::SIGNED: return "int";
	case PrimitiveKind::UNSIGNED: return "uint";
	case PrimitiveKind::FLOAT: return "float";
	}
	return "?";
}

struct Emitter
{
	IRModule& module;
	Context& context;
	Arena* arena = nullptr; // names and expressions: the module's
	EntryPoint* entry_point = nullptr;

	std::vector<ZTStringView> names; // per IRRef: a value's variable or constant, a pointer's lvalue
	std::unordered_map<Type*, ZTStringView> type_names;
	std::vector<Type*> composites; // structs and arrays, in order of first use
	uint32 struct_count = 0;
	uint32 array_count = 0;
	std::vector<uint8> reachable; // per IRRef of the current function's blocks
	uint32 next_value = 0;        // per function
	Type* return_type = nullptr;  // of the current function
	bool in_main = false;         // emitting the entry function, which returns the out struct
	bool has_outputs = false;

	StringBuilder* out = nullptr;
	uint32 depth = 0;

	ZTStringView Print(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		ZTStringView text = avprintf(arena, format, args);
		va_end(args);
		return text;
	}

	// Types

	const char* TypeName(Type* type)
	{
		auto found = type_names.find(type);
		if (found != type_names.end())
			return found->second.CString();
		ZTStringView name;
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE: name = ScalarName(((PrimitiveType*)type)->primitive_kind); break;
		case TypeKind::VECTOR:
		{
			VectorType* vector = (VectorType*)type;
			name = Print("%s%u", ScalarName(vector->element->primitive_kind), vector->count);
			break;
		}
		case TypeKind::MATRIX:
		{
			// MSL's matrices are column-major like the IR's, floatCxR.
			MatrixType* matrix = (MatrixType*)type;
			name = Print("%s%ux%u", ScalarName(matrix->element->primitive_kind), matrix->columns, matrix->rows);
			break;
		}
		case TypeKind::STRUCT:
			name = Print("S%u", struct_count++);
			composites.push_back(type);
			break;
		case TypeKind::ARRAY:
			name = Print("A%u", array_count++);
			composites.push_back(type);
			break;
		default: Panic("MSL: %s isn't a named type", TypeToString(type, arena).CString());
		}
		type_names[type] = name;
		return name.CString();
	}

	// Definitions of the structs and array wrappers used, each after the ones it contains.
	void DefineComposites(StringBuilder& text)
	{
		std::unordered_map<Type*, bool> defined;
		// Defining one can name composites not seen yet, which are added to the list.
		for (size_t i = 0; i < composites.size(); ++i)
			DefineComposite(composites[i], text, defined);
	}

	void DefineComposite(Type* type, StringBuilder& text, std::unordered_map<Type*, bool>& defined)
	{
		if (defined[type])
			return;
		defined[type] = true;
		auto define_part = [&](Type* part) {
			if (part->type_kind == TypeKind::STRUCT || part->type_kind == TypeKind::ARRAY)
			{
				TypeName(part);
				DefineComposite(part, text, defined);
			}
		};
		const char* name = TypeName(type);
		if (type->type_kind == TypeKind::ARRAY)
		{
			ArrayType* array = (ArrayType*)type;
			define_part(array->element);
			text.AppendFormat("struct %s\n{\n\t%s e[%u];\n};\n\n", name, TypeName(array->element), array->length);
			return;
		}
		StructType* structure = (StructType*)type;
		for (uint32 i = 0; i < structure->fields.count; ++i)
			define_part(structure->fields[i].type);
		text.AppendFormat("struct %s\n{\n", name);
		for (uint32 i = 0; i < structure->fields.count; ++i)
			text.AppendFormat("\t%s m%u;\n", TypeName(structure->fields[i].type), i);
		text.Append("};\n\n");
	}

	// Constants

	// The shortest text that parses back to the same float, always with a '.' or an exponent. NaNs, infinities, negative
	// zero and denormals are written as their bits, which Metal keeps exactly.
	void AppendFloat(StringBuilder& text, uint32 bits)
	{
		float value;
		memcpy(&value, &bits, 4);
		bool denormal = (bits & 0x7F800000) == 0 && (bits & 0x007FFFFF) != 0;
		if (!isfinite(value) || denormal || bits == 0x80000000)
		{
			text.AppendFormat("as_type<float>(0x%08Xu)", bits);
			return;
		}
		char digits[64];
		double magnitude = fabs(value);
		if (magnitude == 0.0 || (magnitude >= 1e-4 && magnitude < 1e15))
		{
			for (int decimals = 1; decimals <= 48; ++decimals)
			{
				snprintf(digits, sizeof(digits), "%.*f", decimals, (double)value);
				if (strtof(digits, nullptr) == value)
					break;
			}
		}
		else
		{
			for (int precision = 1; precision <= 9; ++precision)
			{
				snprintf(digits, sizeof(digits), "%.*e", precision - 1, (double)value);
				if (strtof(digits, nullptr) == value)
					break;
			}
		}
		text.AppendFormat(value < 0 ? "(%s)" : "%s", digits);
	}

	void AppendScalar(StringBuilder& text, PrimitiveKind kind, uint32 bits)
	{
		switch (kind)
		{
		case PrimitiveKind::BOOL: text.Append(bits ? "true" : "false"); return;
		case PrimitiveKind::SIGNED:
		{
			int32 value;
			memcpy(&value, &bits, 4);
			if (value == INT32_MIN)
				text.Append("(-2147483647 - 1)");
			else
				text.AppendFormat(value < 0 ? "(%d)" : "%d", value);
			return;
		}
		case PrimitiveKind::UNSIGNED: text.AppendFormat("%uu", bits); return;
		case PrimitiveKind::FLOAT: AppendFloat(text, bits); return;
		case PrimitiveKind::VOID: break;
		}
		Panic("MSL: no constants of void");
	}

	// A constant of type from its bytes. Arrays and structs are initializer lists, only valid in declarations.
	void AppendConstant(StringBuilder& text, Type* type, const uint8* bytes)
	{
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE:
		{
			uint32 bits;
			memcpy(&bits, bytes, 4);
			AppendScalar(text, ((PrimitiveType*)type)->primitive_kind, bits);
			return;
		}
		case TypeKind::VECTOR:
		{
			VectorType* vector = (VectorType*)type;
			text.AppendFormat("%s(", TypeName(type));
			for (uint32 i = 0; i < vector->count; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, vector->element, bytes + i * vector->element->size);
			}
			text.Append(")");
			return;
		}
		case TypeKind::MATRIX:
		{
			MatrixType* matrix = (MatrixType*)type;
			Type* column = GetVectorType(context, matrix->element, matrix->rows);
			text.AppendFormat("%s(", TypeName(type));
			for (uint32 i = 0; i < matrix->columns; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, column, bytes + i * column->size);
			}
			text.Append(")");
			return;
		}
		case TypeKind::ARRAY:
		{
			// The wrapper's braces, then the array's.
			ArrayType* array = (ArrayType*)type;
			text.Append("{ { ");
			for (uint32 i = 0; i < array->length; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, array->element, bytes + (size_t)i * array->stride);
			}
			text.Append(" } }");
			return;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			text.Append("{ ");
			for (uint32 i = 0; i < structure->fields.count; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, structure->fields[i].type, bytes + structure->fields[i].offset);
			}
			text.Append(structure->fields.count ? " }" : "}");
			return;
		}
		default: break;
		}
		Panic("MSL: no constants of %s", TypeToString(type, arena).CString());
	}

	ZTStringView ConstantText(Type* type, const uint8* bytes)
	{
		StringBuilder text(arena);
		AppendConstant(text, type, bytes);
		return text.ToString();
	}

	// Values

	const char* Operand(IRRef ref)
	{
		IRValue& value = module[ref];
		if (value.kind == IRValueKind::CONSTANT && !names[ref].length)
			names[ref] = ConstantText(value.type, value.constant.constant->bytes.data);
		assert(names[ref].length);
		return names[ref].CString();
	}

	// A line of the function at the current depth.
	void Line(const char* format, ...)
	{
		for (uint32 i = 0; i < depth; ++i)
			out->Append("\t");
		va_list args;
		va_start(args, format);
		ZTStringView text = avprintf(arena, format, args);
		va_end(args);
		out->Append(text);
		out->Append("\n");
	}

	// Declares ref's variable with the value of expression.
	void Define(IRRef ref, ZTStringView expression)
	{
		ZTStringView name = Print("v%u", next_value++);
		Line("%s %s = %s;", TypeName(module[ref].type), name.CString(), expression.CString());
		names[ref] = name;
	}

	ZTStringView Binary(Slice<IRRef> o, const char* op) { return Print("%s %s %s", Operand(o[0]), op, Operand(o[1])); }

	// A call-like expression, e.g. float4(a, b) or min(a, b), from operands first on.
	ZTStringView Call(const char* callee, Slice<IRRef> o, uint32 first)
	{
		StringBuilder text(arena);
		text.AppendFormat("%s(", callee);
		for (uint32 i = first; i < o.count; ++i)
			text.AppendFormat(i > first ? ", %s" : "%s", Operand(o[i]));
		text.Append(")");
		return text.ToString();
	}

	// A component-wise operation on two operands, format taking each side, e.g. "%s * %s". On matrices it's done column
	// by column, since MSL's * is the matrix product and / and fmod don't take matrices.
	ZTStringView ComponentWise(Type* type, Slice<IRRef> o, const char* format)
	{
		if (type->type_kind != TypeKind::MATRIX)
			return Print(format, Operand(o[0]), Operand(o[1]));
		MatrixType* matrix = (MatrixType*)type;
		StringBuilder text(arena);
		text.AppendFormat("%s(", TypeName(type));
		for (uint32 i = 0; i < matrix->columns; ++i)
		{
			if (i)
				text.Append(", ");
			text.Append(Print(format, Print("%s[%u]", Operand(o[0]), i).CString(), Print("%s[%u]", Operand(o[1]), i).CString()));
		}
		text.Append(")");
		return text.ToString();
	}

	// How the current function returns: main returns its out struct.
	void Return(const char* value)
	{
		if (in_main && has_outputs)
			Line("return out;");
		else if (value)
			Line("return %s;", value);
		else
			Line("return;");
	}

	void Instruction(IRRef ref)
	{
		IRValue& value = module[ref];
		Slice<IRRef> o = GetIROperands(module, ref);
		Type* type = value.type;
		bool logical = type && type->type_kind != TypeKind::POINTER && ScalarKind(type) == PrimitiveKind::BOOL;
		switch (value.op)
		{
		case IROp::LOAD: Define(ref, Operand(o[0])); break;
		case IROp::STORE:
		case IROp::COPY: Line("%s = %s;", Operand(o[0]), Operand(o[1])); break;
		case IROp::ACCESS:
		{
			StringBuilder lvalue(arena);
			lvalue.Append(Operand(o[0]));
			Type* current = ((PointerType*)module[o[0]].type)->pointee;
			for (uint32 i = 1; i < o.count; ++i)
			{
				switch (current->type_kind)
				{
				case TypeKind::STRUCT:
				{
					uint32 field;
					memcpy(&field, module[o[i]].constant.constant->bytes.data, 4);
					lvalue.AppendFormat(".m%u", field);
					current = ((StructType*)current)->fields[field].type;
					break;
				}
				case TypeKind::ARRAY:
					lvalue.AppendFormat(".e[%s]", Operand(o[i]));
					current = ((ArrayType*)current)->element;
					break;
				case TypeKind::VECTOR:
					lvalue.AppendFormat("[%s]", Operand(o[i]));
					current = ((VectorType*)current)->element;
					break;
				case TypeKind::MATRIX:
				{
					MatrixType* matrix = (MatrixType*)current;
					lvalue.AppendFormat("[%s]", Operand(o[i]));
					current = GetVectorType(context, matrix->element, matrix->rows);
					break;
				}
				default: Panic("MSL: can't access into %s", TypeToString(current, arena).CString());
				}
			}
			names[ref] = lvalue.ToString();
			break;
		}
		case IROp::CONSTRUCT: Define(ref, Call(TypeName(type), o, 0)); break;
		case IROp::EXTRACT:
		case IROp::EXTRACT_DYNAMIC: Define(ref, Print("%s[%s]", Operand(o[0]), Operand(o[1]))); break;
		case IROp::SHUFFLE:
		{
			static const char COMPONENTS[] = "xyzw";
			uint32 first_count = ComponentCount(module[o[0]].type);
			StringBuilder text(arena);
			text.AppendFormat("%s(", TypeName(type));
			for (uint32 i = 2; i < o.count; ++i)
			{
				uint32 index;
				memcpy(&index, module[o[i]].constant.constant->bytes.data, 4);
				bool first = index < first_count;
				text.AppendFormat(i > 2 ? ", %s.%c" : "%s.%c", Operand(o[first ? 0 : 1]), COMPONENTS[first ? index : index - first_count]);
			}
			text.Append(")");
			Define(ref, text.ToString());
			break;
		}
		case IROp::ADD: Define(ref, Binary(o, "+")); break;
		case IROp::SUB: Define(ref, Binary(o, "-")); break;
		case IROp::MUL: Define(ref, ComponentWise(type, o, "%s * %s")); break;
		case IROp::DIV: Define(ref, ComponentWise(type, o, "%s / %s")); break;
		case IROp::REM:
			// fmod truncates like C's integer %, the same as HLSL's % and SPIR-V's OpFRem.
			Define(ref, ComponentWise(type, o, ScalarKind(type) == PrimitiveKind::FLOAT ? "fmod(%s, %s)" : "%s %% %s"));
			break;
		case IROp::NEG: Define(ref, Print("-%s", Operand(o[0]))); break;
		case IROp::AND: Define(ref, Binary(o, logical ? "&&" : "&")); break;
		case IROp::OR: Define(ref, Binary(o, logical ? "||" : "|")); break;
		case IROp::XOR: Define(ref, Binary(o, logical ? "!=" : "^")); break;
		case IROp::NOT: Define(ref, Print("%s%s", logical ? "!" : "~", Operand(o[0]))); break;
		case IROp::SHL: Define(ref, Binary(o, "<<")); break;
		case IROp::SHR: Define(ref, Binary(o, ">>")); break;
		case IROp::EQ: Define(ref, Binary(o, "==")); break;
		case IROp::NE: Define(ref, Binary(o, "!=")); break;
		case IROp::LT: Define(ref, Binary(o, "<")); break;
		case IROp::LE: Define(ref, Binary(o, "<=")); break;
		case IROp::GT: Define(ref, Binary(o, ">")); break;
		case IROp::GE: Define(ref, Binary(o, ">=")); break;
		case IROp::SELECT:
			// ?: takes only a scalar condition; select picks per component.
			if (module[o[0]].type->type_kind == TypeKind::VECTOR)
				Define(ref, Print("select(%s, %s, %s)", Operand(o[2]), Operand(o[1]), Operand(o[0])));
			else
				Define(ref, Print("%s ? %s : %s", Operand(o[0]), Operand(o[1]), Operand(o[2])));
			break;
		case IROp::CONVERT:
			if (module[o[0]].type == type)
				names[ref] = ZTStringView(Operand(o[0]));
			else
				Define(ref, Print("%s(%s)", TypeName(type), Operand(o[0])));
			break;
		case IROp::BITCAST:
			if (module[o[0]].type == type)
				names[ref] = ZTStringView(Operand(o[0]));
			else
				Define(ref, Print("as_type<%s>(%s)", TypeName(type), Operand(o[0])));
			break;
		case IROp::INTRINSIC: Define(ref, Call(IRIntrinsicToString((IRIntrinsic)value.sub_op).CString(), o, 0)); break;
		case IROp::CALL:
		{
			ZTStringView call = Call(names[o[0]].CString(), o, 1);
			if (type)
				Define(ref, call);
			else
				Line("%s;", call.CString());
			break;
		}
		case IROp::RETURN:
			// At the end of a function returning nothing it goes without saying.
			if (o.count || depth > 1 || (in_main && has_outputs))
				Return(o.count ? Operand(o[0]) : nullptr);
			break;
		case IROp::DISCARD: Line("discard_fragment();"); break;
		case IROp::UNREACHABLE:
			// MSL has no way to say so; a value is returned so every path returns one.
			Return(return_type == context.void_type ? nullptr : Print("%s{}", TypeName(return_type)).CString());
			break;
		default: Panic("MSL: can't emit %s", GetIROpInfo(value.op).name);
		}
	}

	// Control flow

	void FindReachableBlocks(IRRef function)
	{
		std::vector<IRRef> stack = { module[function].function.first_block };
		reachable[stack[0]] = 1;
		while (!stack.empty())
		{
			IRRef terminator = module[stack.back()].block.last;
			stack.pop_back();
			IROp op = module[terminator].op;
			if (op != IROp::BRANCH && op != IROp::BRANCH_IF)
				continue;
			Slice<IRRef> o = GetIROperands(module, terminator);
			for (uint32 i = op == IROp::BRANCH ? 0 : 1; i < o.count; ++i)
			{
				if (!reachable[o[i]])
				{
					reachable[o[i]] = 1;
					stack.push_back(o[i]);
				}
			}
		}
	}

	// Prints the blocks from block on until reaching end, the merge block of the construct they're in, or 0 for none.
	// Recurses once per nested if, which the front end's nesting limit bounds.
	void Region(IRRef block, IRRef end)
	{
		while (block && block != end)
		{
			IRRef terminator = module[block].block.last;
			IRRef merge = module[terminator].instruction.prev;
			if (merge && module[merge].op == IROp::LOOP_MERGE)
				Panic("MSL: loops aren't supported yet");
			if (merge && module[merge].op != IROp::SELECTION_MERGE)
				merge = 0;
			for (IRRef instruction = module[block].block.first; instruction != (merge ? merge : terminator);
				 instruction = module[instruction].instruction.next)
				Instruction(instruction);

			Slice<IRRef> o = GetIROperands(module, terminator);
			switch (module[terminator].op)
			{
			case IROp::BRANCH: block = o[0]; break;
			case IROp::BRANCH_IF:
			{
				if (!merge)
					Panic("MSL: a branch_if without a selection_merge");
				IRRef merge_block = GetIROperands(module, merge)[0];
				IRRef else_block = o[2];
				Line("if (%s)", Operand(o[0]));
				Line("{");
				depth++;
				Region(o[1], merge_block);
				depth--;
				Line("}");

				// An else that does nothing, like a block that only branches to the merge, isn't printed.
				StringBuilder else_text(arena);
				StringBuilder* outer = out;
				out = &else_text;
				depth++;
				Region(else_block, merge_block);
				depth--;
				out = outer;
				if (else_text.length)
				{
					Line("else");
					Line("{");
					out->Append(else_text.ToString());
					Line("}");
				}
				// Both sides can end the function, leaving nothing to merge.
				block = reachable[merge_block] ? merge_block : 0;
				break;
			}
			default:
				Instruction(terminator);
				block = 0;
				break;
			}
		}
	}

	// Functions

	void Function(IRRef function, ZTStringView main_signature)
	{
		IRValue& value = module[function];
		IRFunctionInfo* info = value.function.info;
		FunctionType* type = (FunctionType*)value.type;
		next_value = 0;
		return_type = type->return_type;
		in_main = info->entry_point != nullptr;

		if (in_main)
			Line("%s", main_signature.CString());
		else
		{
			StringBuilder signature(arena);
			signature.AppendFormat("%s %s(", TypeName(type->return_type), names[function].CString());
			uint32 index = 0;
			for (IRRef parameter = info->first_parameter; parameter; parameter = module[parameter].parameter.next)
			{
				ZTStringView name = Print("p%u", index);
				Type* parameter_type = module[parameter].type;
				if (index)
					signature.Append(", ");
				if (parameter_type->type_kind == TypeKind::POINTER)
					signature.AppendFormat("thread %s& %s", TypeName(((PointerType*)parameter_type)->pointee), name.CString());
				else
					signature.AppendFormat("%s %s", TypeName(parameter_type), name.CString());
				names[parameter] = name;
				index++;
			}
			Line("%s)", signature.ToString().CString());
		}
		Line("{");
		depth++;
		if (in_main && has_outputs)
			Line("Out out = {};");
		for (IRRef local = info->first_local; local; local = module[local].local.next)
		{
			Type* pointee = ((PointerType*)module[local].type)->pointee;
			ZTStringView name = Print("l%u", module[local].local.index);
			Constant* initializer = module[local].local.initializer;
			ZTStringView initial = initializer ? ConstantText(pointee, initializer->bytes.data) : ZTStringView("{}");
			Line("%s %s = %s;", TypeName(pointee), name.CString(), initial.CString());
			names[local] = name;
		}
		FindReachableBlocks(function);
		Region(value.function.first_block, 0);
		depth--;
		Line("}");
		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
			reachable[block] = 0;
		in_main = false;
	}

	// An interface global's attribute, e.g. [[user(locn0)]].
	ZTStringView Attribute(ShaderIO* io)
	{
		bool input = io->direction == IODirection::INPUT;
		bool vertex = entry_point->stage == ShaderStage::VERTEX;
		if (io->io_kind == IOKind::SEMANTIC)
			return io->semantic == Semantic::VERTEX_INDEX ? "[[vertex_id]]" : "[[position]]";
		if (vertex && input)
			return Print("[[attribute(%u)]]", io->location);
		if (!vertex && !input)
			return Print("[[color(%u)]]", io->location);
		// Integers between stages can't be interpolated.
		bool flat = !vertex && ScalarKind(io->type) != PrimitiveKind::FLOAT;
		return Print("[[user(locn%u)%s]]", io->location, flat ? ", flat" : "");
	}

	ZTStringView Module(IRRef wrapper, Arena* output_arena)
	{
		names.assign(module.count, ZTStringView());
		reachable.assign(module.count, 0);
		entry_point = module[wrapper].function.info->entry_point;
		assert(entry_point);

		IRReachable used;
		FindReachable(module, wrapper, used);
		for (size_t i = 0; i < used.functions.size(); ++i)
			names[used.functions[i]] = Print("f%u", (uint32)i);

		// Interface globals: inputs, then outputs, each sorted by location with semantics last. Metal matches the stages'
		// values by their user(locnN) names, so the order is only for reading.
		std::vector<IRRef> interface_globals;
		std::vector<IRRef> globals;
		for (IRRef global : used.globals)
		{
			AddressSpace space = ((PointerType*)module[global].type)->space;
			if (space == AddressSpace::INPUT || space == AddressSpace::OUTPUT)
				interface_globals.push_back(global);
			else if (space == AddressSpace::CONSTANT)
			{
				names[global] = Print("g%u", (uint32)globals.size());
				globals.push_back(global);
			}
			else
				Panic("MSL: private globals aren't supported yet"); // Metal has no mutable globals, see Docs/IR.md
		}
		auto order = [&](IRRef global) {
			ShaderIO* io = module[global].global.io;
			uint64 direction = io->direction == IODirection::INPUT ? 0 : 1;
			uint64 kind = io->io_kind == IOKind::LOCATION ? 0 : 1;
			uint64 slot = io->io_kind == IOKind::LOCATION ? io->location : (uint32)io->semantic;
			return direction << 40 | kind << 32 | slot;
		};
		std::sort(interface_globals.begin(), interface_globals.end(), [&](IRRef a, IRRef b) { return order(a) < order(b); });

		// The in struct is [[stage_in]]; [[vertex_id]] can only be a parameter.
		StringBuilder in_fields(arena);
		StringBuilder out_fields(arena);
		StringBuilder parameters(arena);
		uint32 inputs = 0;
		uint32 outputs = 0;
		for (IRRef global : interface_globals)
		{
			ShaderIO* io = module[global].global.io;
			const char* type = TypeName(io->type);
			ZTStringView attribute = Attribute(io);
			if (io->direction == IODirection::OUTPUT)
			{
				out_fields.AppendFormat("\t%s out%u %s;\n", type, outputs, attribute.CString());
				names[global] = Print("out.out%u", outputs++);
			}
			else if (io->io_kind == IOKind::SEMANTIC && io->semantic == Semantic::VERTEX_INDEX)
			{
				parameters.AppendFormat("%s%s in%u %s", parameters.length ? ", " : "", type, inputs, attribute.CString());
				names[global] = Print("in%u", inputs++);
			}
			else
			{
				in_fields.AppendFormat("\t%s in%u %s;\n", type, inputs, attribute.CString());
				names[global] = Print("in.in%u", inputs++);
			}
		}
		if (in_fields.length)
			parameters.AppendFormat("%sIn in [[stage_in]]", parameters.length ? ", " : "");
		has_outputs = out_fields.length != 0;
		ZTStringView main_signature = Print("%s %s %s(%s)", entry_point->stage == ShaderStage::VERTEX ? "vertex" : "fragment",
			has_outputs ? "Out" : "void", GPU::MSL_ENTRY_POINT_NAME, parameters.ToString().CString());

		StringBuilder functions(arena);
		out = &functions;
		for (size_t i = 0; i < used.functions.size(); ++i)
		{
			if (i)
				functions.Append("\n");
			Function(used.functions[i], main_signature);
		}

		StringBuilder declarations(arena);
		for (IRRef global : globals)
		{
			PointerType* pointer = (PointerType*)module[global].type;
			Constant* initializer = module[global].global.initializer;
			declarations.AppendFormat("constant %s %s = ", TypeName(pointer->pointee), names[global].CString());
			if (initializer)
				AppendConstant(declarations, pointer->pointee, initializer->bytes.data);
			else
				declarations.Append("{}");
			declarations.Append(";\n");
		}
		if (declarations.length)
			declarations.Append("\n");

		// Composite definitions after the functions and declarations, once every one used has been named.
		StringBuilder text(output_arena);
		text.Append("#include <metal_stdlib>\nusing namespace metal;\n\n");
		DefineComposites(text);
		if (in_fields.length)
			text.AppendFormat("struct In\n{\n%s};\n\n", in_fields.ToString().CString());
		if (out_fields.length)
			text.AppendFormat("struct Out\n{\n%s};\n\n", out_fields.ToString().CString());
		text.Append(declarations.ToString());
		text.Append(functions.ToString());
		return text.ToString();
	}
};

}

ZTStringView EmitMSL(IRModule& module, IRRef wrapper, Arena* arena, std::vector<ScriptError*>& errors)
{
	(void)errors; // no limits of Metal's are checked yet
	Emitter emitter = { .module = module, .context = *module.context, .arena = module.arena };
	return emitter.Module(wrapper, arena);
}

}
