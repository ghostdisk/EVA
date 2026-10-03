#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <unordered_map>

// Emits one entry point as HLSL for fxc at shader model 5.0 (D3D11).
//
// - Every value is a variable assigned once, v0, v1... Pointers, which HLSL doesn't have, are kept as the lvalue text
//   they stand for, like l0.m1[v2]: in shaders every pointer is a variable followed by access steps (Docs/IR.md).
// - Pointer parameters, which the IR uses for arrays and structs, are inout. Nothing else in the call points to the
//   same memory, so copying in and out means the same.
// - Interface globals are parameters of main with their semantics, inputs before outputs, sorted by location with
//   semantics last, so a pixel shader's inputs line up with the vertex shader's outputs in the common cases.
// - Control flow is printed from the IR's structure: an if per selection_merge. Only blocks reachable from the entry
//   block are printed.
// - Only generated names: user identifiers never reach the output.
//
// fxc rejects some code that's valid but that it can fold to something undefined. Out of bounds indices are clamped by
// ClampIndices before this; integer division is printed with a divisor that's never zero; stores to a vector component
// picked at runtime select the whole vector.

namespace EVA::Script
{

namespace
{

// fxc's limit on the elements of an array, all its dimensions together (X3059).
static const uint32 MAX_ARRAY_ELEMENTS = 65536;

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

// A pointer whose last step picks a vector component or matrix row at runtime.
struct DynamicComponent
{
	ZTStringView base; // the vector or matrix
	ZTStringView index;
	Type* composite = nullptr;
};

struct Emitter
{
	IRModule& module;
	Context& context;
	Arena* arena = nullptr; // names and expressions: the module's
	EntryPoint* entry_point = nullptr;

	std::vector<ZTStringView> names; // per IRRef: a value's variable or constant, a pointer's lvalue
	std::unordered_map<IRRef, DynamicComponent> dynamic_components;
	std::unordered_map<Type*, ZTStringView> type_names;
	std::unordered_map<StructType*, uint32> struct_numbers;
	std::vector<StructType*> structs; // in order of first use
	std::vector<uint8> reachable;     // per IRRef of the current function's blocks
	uint32 next_value = 0;            // per function
	Type* return_type = nullptr;      // of the current function

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
			// HLSL's rows are the IR's columns, so indexing gives the same vector.
			MatrixType* matrix = (MatrixType*)type;
			name = Print("%s%ux%u", ScalarName(matrix->element->primitive_kind), matrix->columns, matrix->rows);
			break;
		}
		case TypeKind::STRUCT:
		{
			uint32 number = (uint32)structs.size();
			struct_numbers[(StructType*)type] = number;
			structs.push_back((StructType*)type);
			name = Print("S%u", number);
			break;
		}
		default: Panic("HLSL: %s isn't a named type", TypeToString(type, arena).CString());
		}
		type_names[type] = name;
		return name.CString();
	}

	// The element type under any arrays, and their sizes appended to sizes, e.g. [2][3] for [2][3]float.
	Type* ArrayBase(Type* type, StringBuilder* sizes)
	{
		while (type->type_kind == TypeKind::ARRAY)
		{
			ArrayType* array = (ArrayType*)type;
			if (sizes)
				sizes->AppendFormat("[%u]", array->length);
			type = array->element;
		}
		return type;
	}

	// A declaration of name as type, e.g. float2 name[3].
	ZTStringView Declare(Type* type, const char* name)
	{
		StringBuilder sizes(arena);
		Type* base = ArrayBase(type, &sizes);
		return Print("%s %s%s", TypeName(base), name, sizes.ToString().CString());
	}

	// Zero of type, e.g. (float2[3])0.
	ZTStringView Zero(Type* type)
	{
		StringBuilder sizes(arena);
		Type* base = ArrayBase(type, &sizes);
		return Print("(%s%s)0", TypeName(base), sizes.ToString().CString());
	}

	// Definitions of the structs used, each after the structs it contains.
	void DefineStructs(StringBuilder& text)
	{
		std::vector<uint8> defined;
		// Defining one can name structs not seen yet, which are added to the list.
		for (size_t i = 0; i < structs.size(); ++i)
			DefineStruct(structs[i], text, defined);
	}

	void DefineStruct(StructType* structure, StringBuilder& text, std::vector<uint8>& defined)
	{
		uint32 number = struct_numbers.at(structure);
		if (defined.size() <= number)
			defined.resize(number + 1, 0);
		if (defined[number])
			return;
		defined[number] = 1;
		for (uint32 i = 0; i < structure->fields.count; ++i)
		{
			Type* base = ArrayBase(structure->fields[i].type, nullptr);
			if (base->type_kind == TypeKind::STRUCT)
			{
				TypeName(base);
				DefineStruct((StructType*)base, text, defined);
			}
		}
		text.AppendFormat("struct S%u\n{\n", number);
		for (uint32 i = 0; i < structure->fields.count; ++i)
			text.AppendFormat("\t%s;\n", Declare(structure->fields[i].type, Print("m%u", i).CString()).CString());
		text.Append("};\n\n");
	}

	// Constants

	// The shortest text that parses back to the same float, always with a '.' or an exponent. NaNs, infinities, negative
	// zero and denormals are written as their bits, which fxc keeps exactly.
	void AppendFloat(StringBuilder& text, uint32 bits)
	{
		float value;
		memcpy(&value, &bits, 4);
		bool denormal = (bits & 0x7F800000) == 0 && (bits & 0x007FFFFF) != 0;
		if (!isfinite(value) || denormal || bits == 0x80000000)
		{
			text.AppendFormat("asfloat(0x%08Xu)", bits);
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
		Panic("HLSL: no constants of void");
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
			ArrayType* array = (ArrayType*)type;
			text.Append("{ ");
			for (uint32 i = 0; i < array->length; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, array->element, bytes + (size_t)i * array->stride);
			}
			text.Append(" }");
			return;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			if (!structure->fields.count)
			{
				text.Append(Zero(type));
				return;
			}
			text.Append("{ ");
			for (uint32 i = 0; i < structure->fields.count; ++i)
			{
				if (i)
					text.Append(", ");
				AppendConstant(text, structure->fields[i].type, bytes + structure->fields[i].offset);
			}
			text.Append(" }");
			return;
		}
		default: break;
		}
		Panic("HLSL: no constants of %s", TypeToString(type, arena).CString());
	}

	ZTStringView ConstantText(Type* type, const uint8* bytes)
	{
		StringBuilder text(arena);
		AppendConstant(text, type, bytes);
		return text.ToString();
	}

	// Whether every component of a constant is nonzero.
	bool NonZeroConstant(IRRef ref)
	{
		IRValue& value = module[ref];
		if (value.kind != IRValueKind::CONSTANT)
			return false;
		Slice<uint8> bytes = value.constant.constant->bytes;
		for (uint32 i = 0; i + 4 <= bytes.count; i += 4)
		{
			uint32 bits;
			memcpy(&bits, bytes.data + i, 4);
			if (!bits)
				return false;
		}
		return true;
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

	// fxc can't store to a vector component or a matrix row picked at runtime, "not natively addressable". Vectors get the
	// whole vector selected per component, matrices a chain of ifs with constant indices.
	void StoreDynamicComponent(const DynamicComponent& component, const char* value)
	{
		const char* base = component.base.CString();
		const char* index = component.index.CString();
		if (component.composite->type_kind == TypeKind::VECTOR)
		{
			VectorType* vector = (VectorType*)component.composite;
			StringBuilder indices(arena);
			for (uint32 i = 0; i < vector->count; ++i)
				indices.AppendFormat(i ? ", %uu" : "%uu", i);
			Line("%s = %s == uint%u(%s) ? (%s)%s : %s;", base, index, vector->count, indices.ToString().CString(),
				TypeName(vector), value, base);
			return;
		}
		MatrixType* matrix = (MatrixType*)component.composite;
		for (uint32 i = 0; i < matrix->columns; ++i)
		{
			Line("%sif (%s == %uu)", i ? "else " : "", index, i);
			Line("\t%s[%u] = %s;", base, i, value);
		}
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
		{
			auto component = dynamic_components.find(o[0]);
			if (component != dynamic_components.end())
				StoreDynamicComponent(component->second, Operand(o[1]));
			else
				Line("%s = %s;", Operand(o[0]), Operand(o[1]));
			break;
		}
		case IROp::COPY: Line("%s = %s;", Operand(o[0]), Operand(o[1])); break;
		case IROp::ACCESS:
		{
			StringBuilder lvalue(arena);
			lvalue.Append(Operand(o[0]));
			Type* current = ((PointerType*)module[o[0]].type)->pointee;
			for (uint32 i = 1; i < o.count; ++i)
			{
				if (current->type_kind == TypeKind::STRUCT)
				{
					uint32 field;
					memcpy(&field, module[o[i]].constant.constant->bytes.data, 4);
					lvalue.AppendFormat(".m%u", field);
					current = ((StructType*)current)->fields[field].type;
					continue;
				}
				bool dynamic = module[o[i]].kind != IRValueKind::CONSTANT;
				bool component = current->type_kind == TypeKind::VECTOR || current->type_kind == TypeKind::MATRIX;
				if (dynamic && component && i == o.count - 1)
				{
					ZTStringView base = InternString(arena, lvalue.ToString());
					dynamic_components[ref] = { .base = base, .index = names[o[i]], .composite = current };
				}
				lvalue.AppendFormat("[%s]", Operand(o[i]));
				switch (current->type_kind)
				{
				case TypeKind::ARRAY: current = ((ArrayType*)current)->element; break;
				case TypeKind::VECTOR: current = ((VectorType*)current)->element; break;
				case TypeKind::MATRIX:
				{
					MatrixType* matrix = (MatrixType*)current;
					current = GetVectorType(context, matrix->element, matrix->rows);
					break;
				}
				default: Panic("HLSL: can't access into %s", TypeToString(current, arena).CString());
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
		case IROp::MUL: Define(ref, Binary(o, "*")); break;
		case IROp::DIV:
		case IROp::REM:
		{
			const char* op = value.op == IROp::DIV ? "/" : "%";
			if (ScalarKind(type) == PrimitiveKind::FLOAT || NonZeroConstant(o[1]))
			{
				Define(ref, Binary(o, op));
				break;
			}
			// fxc rejects integer division it can fold to a division by zero, so the divisor never is: x / 0 is x.
			const char* b = Operand(o[1]);
			const char* name = TypeName(type);
			Define(ref, Print("%s %s (%s == (%s)0 ? (%s)1 : %s)", Operand(o[0]), op, b, name, name, b));
			break;
		}
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
		case IROp::SELECT: Define(ref, Print("%s ? %s : %s", Operand(o[0]), Operand(o[1]), Operand(o[2]))); break;
		case IROp::CONVERT:
			if (module[o[0]].type == type)
				names[ref] = ZTStringView(Operand(o[0]));
			else
				Define(ref, Print("(%s)%s", TypeName(type), Operand(o[0])));
			break;
		case IROp::BITCAST:
		{
			if (module[o[0]].type == type)
			{
				names[ref] = ZTStringView(Operand(o[0]));
				break;
			}
			PrimitiveKind kind = ScalarKind(type);
			const char* function = kind == PrimitiveKind::FLOAT ? "asfloat" : kind == PrimitiveKind::SIGNED ? "asint" : "asuint";
			Define(ref, Print("%s(%s)", function, Operand(o[0])));
			break;
		}
		case IROp::MATMUL:
			// HLSL's matrices are the IR's transposed, so the operands swap.
			Define(ref, Print("mul(%s, %s)", Operand(o[1]), Operand(o[0])));
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
			if (o.count)
				Line("return %s;", Operand(o[0]));
			else if (depth > 1) // at the end of the function it goes without saying
				Line("return;");
			break;
		case IROp::DISCARD: Line("discard;"); break;
		case IROp::UNREACHABLE:
			// HLSL has no way to say so, and fxc wants a value returned on every path.
			if (return_type == context.void_type)
				Line("return;");
			else
				Line("return %s;", Zero(return_type).CString());
			break;
		default: Panic("HLSL: can't emit %s", GetIROpInfo(value.op).name);
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
				Panic("HLSL: loops aren't supported yet");
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
					Panic("HLSL: a branch_if without a selection_merge");
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

	void Function(IRRef function, Slice<IRRef> interface_globals)
	{
		IRValue& value = module[function];
		IRFunctionInfo* info = value.function.info;
		FunctionType* type = (FunctionType*)value.type;
		next_value = 0;
		return_type = type->return_type;

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
				signature.AppendFormat("inout %s", Declare(((PointerType*)parameter_type)->pointee, name.CString()).CString());
			else
				signature.AppendFormat("%s %s", TypeName(parameter_type), name.CString());
			names[parameter] = name;
			index++;
		}
		if (info->entry_point)
		{
			for (uint32 i = 0; i < interface_globals.count; ++i)
				signature.AppendFormat(i ? ", %s" : "%s", InterfaceParameter(interface_globals[i]).CString());
		}
		Line("%s)", signature.ToString().CString());
		Line("{");
		depth++;
		for (IRRef local = info->first_local; local; local = module[local].local.next)
		{
			Type* pointee = ((PointerType*)module[local].type)->pointee;
			ZTStringView name = Print("l%u", module[local].local.index);
			Constant* initializer = module[local].local.initializer;
			ZTStringView initial = initializer ? ConstantText(pointee, initializer->bytes.data) : Zero(pointee);
			Line("%s = %s;", Declare(pointee, name.CString()).CString(), initial.CString());
			names[local] = name;
		}
		FindReachableBlocks(function);
		Region(value.function.first_block, 0);
		depth--;
		Line("}");
		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
			reachable[block] = 0;
	}

	// An interface global as a parameter of main, e.g. out float4 out0 : SV_Position.
	ZTStringView InterfaceParameter(IRRef global)
	{
		ShaderIO* io = module[global].global.io;
		bool input = io->direction == IODirection::INPUT;
		bool vertex = entry_point->stage == ShaderStage::VERTEX;
		ZTStringView semantic;
		if (io->io_kind == IOKind::SEMANTIC)
			semantic = io->semantic == Semantic::VERTEX_INDEX ? "SV_VertexID" : "SV_Position";
		else if (vertex && input)
			semantic = Print("ATTRIB%u", io->location);
		else if (!vertex && !input)
			semantic = Print("SV_Target%u", io->location);
		else
			semantic = Print("TEXCOORD%u", io->location);

		// Integers between stages can't be interpolated.
		bool between_stages = io->io_kind == IOKind::LOCATION && vertex != input;
		bool flat = between_stages && ScalarKind(io->type) != PrimitiveKind::FLOAT;
		return Print("%s%s%s %s : %s", flat ? "nointerpolation " : "", input ? "" : "out ", TypeName(io->type),
			names[global].CString(), semantic.CString());
	}

	// The most elements of any array in type, counting an array of arrays as one, since fxc's limit is on all its
	// dimensions together. 0 if there's none. Structs are looked up once, so a type that repeats a struct many times over
	// isn't walked exponentially.
	std::unordered_map<StructType*, uint64> struct_largest_arrays;

	uint64 LargestArray(Type* type)
	{
		if (type->type_kind == TypeKind::ARRAY)
		{
			// Below 2^64: every array type is under 4 GB, and its elements take at least a byte.
			uint64 elements = 1;
			while (type->type_kind == TypeKind::ARRAY)
			{
				elements *= ((ArrayType*)type)->length;
				type = ((ArrayType*)type)->element;
			}
			uint64 inner = LargestArray(type);
			return elements > inner ? elements : inner;
		}
		if (type->type_kind != TypeKind::STRUCT)
			return 0;
		StructType* structure = (StructType*)type;
		auto found = struct_largest_arrays.find(structure);
		if (found != struct_largest_arrays.end())
			return found->second;
		uint64 largest = 0;
		for (uint32 i = 0; i < structure->fields.count; ++i)
		{
			uint64 field = LargestArray(structure->fields[i].type);
			largest = field > largest ? field : largest;
		}
		struct_largest_arrays[structure] = largest;
		return largest;
	}

	// Arrays live in globals, locals and pointer parameters. An array larger than fxc allows anywhere among those used is
	// reported before printing anything, so a huge constant isn't printed for nothing.
	uint64 LargestUsedArray(IRReachable& used)
	{
		uint64 largest = 0;
		auto check = [&](Type* pointer) {
			if (pointer->type_kind != TypeKind::POINTER)
				return;
			uint64 elements = LargestArray(((PointerType*)pointer)->pointee);
			largest = elements > largest ? elements : largest;
		};
		for (IRRef global : used.globals)
			check(module[global].type);
		for (IRRef function : used.functions)
		{
			IRFunctionInfo* info = module[function].function.info;
			for (IRRef local = info->first_local; local; local = module[local].local.next)
				check(module[local].type);
			for (IRRef parameter = info->first_parameter; parameter; parameter = module[parameter].parameter.next)
				check(module[parameter].type);
		}
		return largest;
	}

	ZTStringView Module(IRRef wrapper, Arena* output_arena, std::vector<ScriptError*>& errors)
	{
		names.assign(module.count, ZTStringView());
		reachable.assign(module.count, 0);
		entry_point = module[wrapper].function.info->entry_point;
		assert(entry_point);

		IRReachable used;
		FindReachable(module, wrapper, used);
		uint64 largest = LargestUsedArray(used);
		if (largest > MAX_ARRAY_ELEMENTS)
		{
			ScriptError* error = output_arena->New<ScriptError>();
			error->message = aprintf(output_arena, "an array of %llu elements is too large for HLSL, the limit is %u",
				(unsigned long long)largest, MAX_ARRAY_ELEMENTS);
			errors.push_back(error);
			return {};
		}
		for (size_t i = 0; i < used.functions.size(); ++i)
			names[used.functions[i]] = used.functions[i] == wrapper ? ZTStringView("main") : Print("f%u", (uint32)i);

		// Interface globals: inputs, then outputs, each sorted by location with semantics last.
		std::vector<IRRef> interface_globals;
		std::vector<IRRef> globals;
		for (IRRef global : used.globals)
		{
			AddressSpace space = ((PointerType*)module[global].type)->space;
			if (space == AddressSpace::INPUT || space == AddressSpace::OUTPUT)
				interface_globals.push_back(global);
			else
			{
				names[global] = Print("g%u", (uint32)globals.size());
				globals.push_back(global);
			}
		}
		auto order = [&](IRRef global) {
			ShaderIO* io = module[global].global.io;
			uint64 direction = io->direction == IODirection::INPUT ? 0 : 1;
			uint64 kind = io->io_kind == IOKind::LOCATION ? 0 : 1;
			uint64 slot = io->io_kind == IOKind::LOCATION ? io->location : (uint32)io->semantic;
			return direction << 40 | kind << 32 | slot;
		};
		std::sort(interface_globals.begin(), interface_globals.end(), [&](IRRef a, IRRef b) { return order(a) < order(b); });
		uint32 inputs = 0;
		uint32 outputs = 0;
		for (IRRef global : interface_globals)
		{
			bool input = module[global].global.io->direction == IODirection::INPUT;
			names[global] = input ? Print("in%u", inputs++) : Print("out%u", outputs++);
		}

		StringBuilder functions(arena);
		out = &functions;
		for (size_t i = 0; i < used.functions.size(); ++i)
		{
			if (i)
				functions.Append("\n");
			Function(used.functions[i], Slice<IRRef>(interface_globals.data(), (uint32)interface_globals.size()));
		}

		StringBuilder declarations(arena);
		for (IRRef global : globals)
		{
			PointerType* pointer = (PointerType*)module[global].type;
			Constant* initializer = module[global].global.initializer;
			declarations.Append(pointer->space == AddressSpace::CONSTANT ? "static const " : "static ");
			declarations.Append(Declare(pointer->pointee, names[global].CString()));
			declarations.Append(" = ");
			if (initializer)
				AppendConstant(declarations, pointer->pointee, initializer->bytes.data);
			else
				declarations.Append(Zero(pointer->pointee));
			declarations.Append(";\n");
		}
		if (declarations.length)
			declarations.Append("\n");

		// Struct definitions last, once every struct used has been named.
		StringBuilder text(output_arena);
		DefineStructs(text);
		text.Append(declarations.ToString());
		text.Append(functions.ToString());
		return text.ToString();
	}
};

}

ZTStringView EmitHLSL(IRModule& module, IRRef wrapper, Arena* arena, std::vector<ScriptError*>& errors)
{
	Emitter emitter = { .module = module, .context = *module.context, .arena = module.arena };
	return emitter.Module(wrapper, arena, errors);
}

}
