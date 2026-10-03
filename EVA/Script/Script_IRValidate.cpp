#include <EVA/Script/Script_IR.hpp>
#include <stdarg.h>

// TODO: check the full structured control flow rules (SPIR-V's 2.11) once IR gen makes ifs and loops: constructs nest,
// branches only leave a construct to its merge, or a loop's continue target or merge.

namespace EVA::Script
{

namespace
{

static bool IsScalar(Type* type)
{
	return type && type->type_kind == TypeKind::PRIMITIVE && ((PrimitiveType*)type)->primitive_kind != PrimitiveKind::VOID;
}

static bool IsScalarOrVector(Type* type)
{
	return IsScalar(type) || (type && type->type_kind == TypeKind::VECTOR);
}

// Types instructions can produce and take.
static bool IsValueType(Type* type)
{
	return IsScalarOrVector(type) || (type && (type->type_kind == TypeKind::MATRIX || type->type_kind == TypeKind::POINTER));
}

static PrimitiveKind ComponentKind(Type* type)
{
	return ComponentType(type)->primitive_kind;
}

static bool IsNumeric(Type* type)
{
	return IsScalarOrVector(type) && ComponentKind(type) != PrimitiveKind::BOOL;
}

static bool IsInteger(Type* type)
{
	if (!IsScalarOrVector(type))
		return false;
	PrimitiveKind kind = ComponentKind(type);
	return kind == PrimitiveKind::SIGNED || kind == PrimitiveKind::UNSIGNED;
}

static bool IsIntegerScalar(Type* type)
{
	return IsScalar(type) && IsInteger(type);
}

static bool IsBoolScalarOrVector(Type* type)
{
	return IsScalarOrVector(type) && ComponentKind(type) == PrimitiveKind::BOOL;
}

static bool IsFloatVector(Type* type)
{
	return type && type->type_kind == TypeKind::VECTOR && ComponentKind(type) == PrimitiveKind::FLOAT;
}

static bool IsFloatMatrix(Type* type)
{
	return type && type->type_kind == TypeKind::MATRIX;
}

struct Validator
{
	IRModule& module;
	Arena* arena;
	ZTStringView error;

	// Where we are, for messages.
	IRRef function = 0;
	uint32 block_number = 0;
	uint32 instruction_number = 0;
	IRRef instruction = 0;
	IRValue invalid = {}; // stands in for broken references while checking lists

	// Per value, for the function being checked.
	std::vector<uint32> block_index;      // BLOCK: its position in the function, plus 1
	std::vector<uint32> position;         // INSTRUCTION: its position in its block
	std::vector<std::vector<uint32>> successors;
	std::vector<int32> idom;              // per block index, -1 if unreachable
	std::vector<uint32> rpo_number;
	std::vector<IRRef> blocks;

	bool Fail(const char* format, ...)
	{
		if (error.length)
			return false;
		va_list args;
		va_start(args, format);
		ZTStringView message = avprintf(arena, format, args);
		va_end(args);
		if (instruction)
			error = aprintf(arena, "@%s block%u, instruction %u (%s): %s", Name(module[function].function.info->name),
				block_number, instruction_number, GetIROpInfo(module[instruction].op).name, message.CString());
		else if (function)
			error = aprintf(arena, "@%s: %s", Name(module[function].function.info->name), message.CString());
		else
			error = message;
		return false;
	}

	const char* Name(Atom atom) { return GetAtomString(atom, arena).CString(); }
	const char* TypeName(Type* type) { return type ? TypeToString(type, arena).CString() : "nothing"; }

	bool Valid(IRRef ref) { return ref && ref < module.count && module[ref].kind != IRValueKind::FREE; }

	// A value of type expected.
	bool Expect(Type* type, Type* expected, const char* what)
	{
		if (type == expected)
			return true;
		return Fail("%s is %s, expected %s", what, TypeName(type), TypeName(expected));
	}

	Type* BoolLike(Type* type)
	{
		Context& context = *module.context;
		if (type->type_kind == TypeKind::VECTOR)
			return GetVectorType(context, context.bool_type, ((VectorType*)type)->count);
		return context.bool_type;
	}

	// A uint constant operand, for indices that have to be known.
	bool ConstantIndex(IRRef ref, uint32 limit, const char* what, uint32* out)
	{
		IRValue& value = module[ref];
		if (value.kind != IRValueKind::CONSTANT || value.type != module.context->uint_type)
			return Fail("%s has to be a uint constant", what);
		uint32 index;
		memcpy(&index, value.constant.constant->bytes.data, 4);
		if (index >= limit)
			return Fail("%s %u is out of range, there are %u", what, index, limit);
		*out = index;
		return true;
	}

	bool Globals()
	{
		uint32 seen = 0;
		for (IRRef global = module.first_global; global; global = module[global].global.next)
		{
			if (!Valid(global) || module[global].kind != IRValueKind::GLOBAL || ++seen > module.count)
				return Fail("the global list is broken");
			IRValue& value = module[global];
			const char* name = Name(value.global.name);
			if (!value.type || value.type->type_kind != TypeKind::POINTER)
				return Fail("@%s's type isn't a pointer", name);
			PointerType* pointer = (PointerType*)value.type;
			if (pointer->space == AddressSpace::FUNCTION || pointer->space == AddressSpace::MEMORY)
				return Fail("@%s is in %s space", name, AddressSpaceToString(pointer->space).CString());
			if (pointer->pointee->type_kind == TypeKind::FUNCTION || pointer->pointee == module.context->void_type)
				return Fail("@%s points to %s", name, TypeName(pointer->pointee));
			bool interface = pointer->space == AddressSpace::INPUT || pointer->space == AddressSpace::OUTPUT;
			if (interface && !value.global.io)
				return Fail("@%s has no shader IO", name);
			if (!interface && value.global.initializer && value.global.initializer->type != pointer->pointee)
				return Fail("@%s's initializer is %s", name, TypeName(value.global.initializer->type));
			if (pointer->space == AddressSpace::CONSTANT && !value.global.initializer)
				return Fail("constant @%s has no initializer", name);
		}
		if (module.last_global && module[module.last_global].global.next)
			return Fail("the global list doesn't end at its last global");
		return true;
	}

	// The function's parameters, locals and blocks, and each block's instructions.
	bool Lists(FunctionType* type, IRFunctionInfo* info)
	{
		uint32 index = 0;
		IRRef parameter = info->first_parameter;
		for (; parameter && index <= info->parameter_count; parameter = module[parameter].parameter.next, ++index)
		{
			IRValue& value = Valid(parameter) ? module[parameter] : invalid;
			if (value.kind != IRValueKind::PARAMETER || value.parent != function || value.parameter.index != index)
				return Fail("parameter %u is broken", index);
			if (index >= type->parameters.count || value.type != type->parameters[index])
				return Fail("parameter %u doesn't match the function's type", index);
		}
		if (parameter || index != type->parameters.count || info->parameter_count != index)
			return Fail("has %u parameters, its type %u", index, type->parameters.count);

		index = 0;
		IRRef local = info->first_local;
		for (; local && index <= info->local_count; local = module[local].local.next, ++index)
		{
			IRValue& value = Valid(local) ? module[local] : invalid;
			if (value.kind != IRValueKind::LOCAL || value.parent != function || value.local.index != index)
				return Fail("local %u is broken", index);
			PointerType* pointer = value.type && value.type->type_kind == TypeKind::POINTER ? (PointerType*)value.type : nullptr;
			if (!pointer || pointer->space != AddressSpace::FUNCTION)
				return Fail("local $%u's type is %s", index, TypeName(value.type));
			if (pointer->pointee->type_kind == TypeKind::FUNCTION || pointer->pointee == module.context->void_type)
				return Fail("local $%u points to %s", index, TypeName(pointer->pointee));
			if (value.local.initializer && value.local.initializer->type != pointer->pointee)
				return Fail("local $%u's initializer is %s", index, TypeName(value.local.initializer->type));
		}
		if (local || info->local_count != index)
			return Fail("has %u locals, counted %u", index, info->local_count);

		blocks.clear();
		IRRef prev = 0;
		for (IRRef block = module[function].function.first_block; block; block = module[block].block.next)
		{
			if (!Valid(block) || module[block].kind != IRValueKind::BLOCK || module[block].parent != function ||
				module[block].block.prev != prev || blocks.size() >= module.count)
				return Fail("the block list is broken");
			blocks.push_back(block);
			block_index[block] = (uint32)blocks.size();
			prev = block;
		}
		if (blocks.empty())
			return Fail("has no blocks");
		if (module[function].function.last_block != prev)
			return Fail("the block list doesn't end at its last block");

		for (uint32 b = 0; b < blocks.size(); ++b)
		{
			block_number = b;
			IRRef block = blocks[b];
			IRRef previous = 0;
			uint32 count = 0;
			for (IRRef ref = module[block].block.first; ref; ref = module[ref].instruction.next)
			{
				if (!Valid(ref) || module[ref].kind != IRValueKind::INSTRUCTION || module[ref].parent != block ||
					module[ref].instruction.prev != previous || count >= module.count)
					return Fail("block%u's instruction list is broken", b);
				position[ref] = count++;
				previous = ref;
			}
			if (module[block].block.last != previous)
				return Fail("block%u's instruction list doesn't end at its last instruction", b);
			if (!previous || module[previous].op >= IROp::COUNT || !(GetIROpInfo(module[previous].op).flags & IR_OP_TERMINATOR))
				return Fail("block%u doesn't end with a terminator", b);
		}
		return true;
	}

	// A block operand of the current function.
	bool Target(IRRef ref, uint32* out_index)
	{
		if (module[ref].kind != IRValueKind::BLOCK || module[ref].parent != function)
			return Fail("expected a block of this function");
		*out_index = block_index[ref] - 1;
		return true;
	}

	// Each block's successors, from its terminator.
	bool Successors()
	{
		successors.assign(blocks.size(), {});
		for (uint32 b = 0; b < blocks.size(); ++b)
		{
			IRRef terminator = module[blocks[b]].block.last;
			Slice<IRRef> operands = GetIROperands(module, terminator);
			IROp op = module[terminator].op;
			if (op != IROp::BRANCH && op != IROp::BRANCH_IF)
				continue;
			uint32 first = op == IROp::BRANCH ? 0 : 1;
			for (uint32 i = first; i < operands.count; ++i)
			{
				IRRef target = operands[i];
				if (!Valid(target) || module[target].kind != IRValueKind::BLOCK || module[target].parent != function)
					return Fail("block%u branches to something that isn't one of the function's blocks", b);
				successors[b].push_back(block_index[target] - 1);
			}
		}
		return true;
	}

	// Cooper, Harvey and Kennedy's iterative dominators, over reverse postorder from the entry block.
	void Dominators()
	{
		uint32 count = (uint32)blocks.size();
		std::vector<uint32> postorder;
		std::vector<uint8> visited(count, 0);
		std::vector<std::pair<uint32, uint32>> stack = { { 0, 0 } };
		visited[0] = 1;
		while (!stack.empty())
		{
			auto& [block, next] = stack.back();
			if (next < successors[block].size())
			{
				uint32 successor = successors[block][next++];
				if (!visited[successor])
				{
					visited[successor] = 1;
					stack.push_back({ successor, 0 });
				}
				continue;
			}
			postorder.push_back(block);
			stack.pop_back();
		}

		rpo_number.assign(count, UINT32_MAX);
		for (uint32 i = 0; i < postorder.size(); ++i)
			rpo_number[postorder[i]] = (uint32)postorder.size() - 1 - i;
		std::vector<std::vector<uint32>> predecessors(count);
		for (uint32 b = 0; b < count; ++b)
		{
			for (uint32 successor : successors[b])
				predecessors[successor].push_back(b);
		}

		idom.assign(count, -1);
		idom[0] = 0;
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (size_t i = postorder.size(); i-- > 0;)
			{
				uint32 block = postorder[i];
				if (block == 0)
					continue;
				int32 new_idom = -1;
				for (uint32 predecessor : predecessors[block])
				{
					if (idom[predecessor] < 0)
						continue;
					if (new_idom < 0)
					{
						new_idom = (int32)predecessor;
						continue;
					}
					uint32 a = predecessor;
					uint32 b = (uint32)new_idom;
					while (a != b)
					{
						while (rpo_number[a] > rpo_number[b])
							a = (uint32)idom[a];
						while (rpo_number[b] > rpo_number[a])
							b = (uint32)idom[b];
					}
					new_idom = (int32)a;
				}
				if (idom[block] != new_idom)
				{
					idom[block] = new_idom;
					changed = true;
				}
			}
		}
	}

	bool Dominates(uint32 a, uint32 b)
	{
		if (idom[b] < 0)
			return true; // nothing is checked in unreachable blocks
		while (b != a && b != 0)
			b = (uint32)idom[b];
		return b == a;
	}

	// An operand that's a value, not a block or function, belonging to this function and available here.
	bool ValueOperand(IRRef ref, uint32 index)
	{
		if (!Valid(ref))
			return Fail("operand %u isn't a value", index);
		IRValue& value = module[ref];
		switch (value.kind)
		{
		case IRValueKind::INSTRUCTION:
		{
			if (!value.type)
				return Fail("operand %u is an instruction without a result", index);
			IRRef block = value.parent;
			if (!Valid(block) || module[block].parent != function)
				return Fail("operand %u is from another function", index);
			uint32 def = block_index[block] - 1;
			uint32 use = block_index[module[instruction].parent] - 1;
			bool available = def == use ? position[ref] < position[instruction] : Dominates(def, use);
			if (!available)
				return Fail("operand %u is used before it's defined", index);
			return true;
		}
		case IRValueKind::PARAMETER:
		case IRValueKind::LOCAL:
			if (value.parent != function)
				return Fail("operand %u is from another function", index);
			return true;
		case IRValueKind::GLOBAL:
		case IRValueKind::CONSTANT: return true;
		default: return Fail("operand %u isn't a value", index);
		}
	}

	bool Instruction(IRRef ref, FunctionType* function_type)
	{
		IRValue& value = module[ref];
		if (value.op == IROp::NONE || value.op >= IROp::COUNT)
			return Fail("op %u", (uint32)value.op);
		const IROpInfo& info = GetIROpInfo(value.op);
		if (info.flags & IR_OP_RESERVED)
			return Fail("isn't supported yet");
		Slice<IRRef> o = GetIROperands(module, ref);
		if (o.count < info.min_operands || (info.max_operands != IR_VARIADIC && o.count > info.max_operands))
			return Fail("has the wrong number of operands, %u", o.count);
		Type* type = value.type;
		if (value.op != IROp::CALL)
		{
			bool result = info.flags & IR_OP_RESULT;
			if (result && !IsValueType(type))
				return Fail("produces %s", TypeName(type));
			if (!result && type)
				return Fail("has a result type");
		}

		// Blocks and functions only where they're expected, everything else has to be an available value.
		for (uint32 i = 0; i < o.count; ++i)
		{
			bool is_block = (value.op == IROp::BRANCH || value.op == IROp::SELECTION_MERGE || value.op == IROp::LOOP_MERGE) ||
							(value.op == IROp::BRANCH_IF && i > 0);
			bool is_function = value.op == IROp::CALL && i == 0;
			uint32 target = 0;
			if (is_block && (!Valid(o[i]) || !Target(o[i], &target)))
				return false;
			if (is_function && (!Valid(o[i]) || module[o[i]].kind != IRValueKind::FUNCTION))
				return Fail("calls something that isn't a function");
			if (!is_block && !is_function && !ValueOperand(o[i], i))
				return false;
		}

		auto types = [&](uint32 i) { return module[o[i]].type; };
		switch (value.op)
		{
		case IROp::LOAD:
		case IROp::STORE:
		{
			PointerType* pointer = types(0)->type_kind == TypeKind::POINTER ? (PointerType*)types(0) : nullptr;
			if (!pointer)
				return Fail("expected a pointer, got %s", TypeName(types(0)));
			if (!IsValueType(pointer->pointee))
				return Fail("can't %s %s whole", info.name, TypeName(pointer->pointee));
			if (value.op == IROp::LOAD)
				return Expect(type, pointer->pointee, "the result");
			if (pointer->space == AddressSpace::CONSTANT || pointer->space == AddressSpace::INPUT)
				return Fail("can't store to %s space", AddressSpaceToString(pointer->space).CString());
			return Expect(types(1), pointer->pointee, "the value");
		}
		case IROp::ACCESS:
		{
			PointerType* pointer = types(0)->type_kind == TypeKind::POINTER ? (PointerType*)types(0) : nullptr;
			if (!pointer)
				return Fail("expected a pointer, got %s", TypeName(types(0)));
			Type* current = pointer->pointee;
			for (uint32 i = 1; i < o.count; ++i)
			{
				uint32 index = 0;
				switch (current->type_kind)
				{
				case TypeKind::STRUCT:
				{
					StructType* structure = (StructType*)current;
					if (!ConstantIndex(o[i], structure->fields.count, "the field index", &index))
						return false;
					current = structure->fields[index].type;
					break;
				}
				case TypeKind::ARRAY:
				case TypeKind::VECTOR:
				case TypeKind::MATRIX:
					if (!IsIntegerScalar(types(i)))
						return Fail("index %u is %s", i, TypeName(types(i)));
					if (current->type_kind == TypeKind::ARRAY)
						current = ((ArrayType*)current)->element;
					else if (current->type_kind == TypeKind::VECTOR)
						current = ((VectorType*)current)->element;
					else
						current = GetVectorType(*module.context, ((MatrixType*)current)->element, ((MatrixType*)current)->rows);
					break;
				default: return Fail("can't access into %s", TypeName(current));
				}
			}
			return Expect(type, GetPointerType(*module.context, pointer->space, current), "the result");
		}
		case IROp::COPY:
		{
			bool pointers = types(0)->type_kind == TypeKind::POINTER && types(1)->type_kind == TypeKind::POINTER;
			if (!pointers || ((PointerType*)types(0))->pointee != ((PointerType*)types(1))->pointee)
				return Fail("copies %s to %s", TypeName(types(1)), TypeName(types(0)));
			AddressSpace space = ((PointerType*)types(0))->space;
			if (space == AddressSpace::CONSTANT || space == AddressSpace::INPUT)
				return Fail("can't copy to %s space", AddressSpaceToString(space).CString());
			return true;
		}
		case IROp::CONSTRUCT:
		{
			assert(type); // it has IR_OP_RESULT, checked above
			if (type->type_kind == TypeKind::VECTOR)
			{
				VectorType* vector = (VectorType*)type;
				uint32 components = 0;
				for (uint32 i = 0; i < o.count; ++i)
				{
					Type* part = types(i);
					if (!IsScalarOrVector(part) || ComponentType(part) != vector->element)
						return Fail("can't construct %s from %s", TypeName(type), TypeName(part));
					components += ComponentCount(part);
				}
				if (components != vector->count)
					return Fail("constructs %s from %u components", TypeName(type), components);
				return true;
			}
			if (type->type_kind == TypeKind::MATRIX)
			{
				MatrixType* matrix = (MatrixType*)type;
				if (o.count != matrix->columns)
					return Fail("constructs %s from %u columns", TypeName(type), o.count);
				Type* column = GetVectorType(*module.context, matrix->element, matrix->rows);
				for (uint32 i = 0; i < o.count; ++i)
				{
					if (!Expect(types(i), column, "a column"))
						return false;
				}
				return true;
			}
			return Fail("can't construct %s", TypeName(type));
		}
		case IROp::EXTRACT:
		{
			uint32 index = 0;
			Type* composite = types(0);
			if (composite->type_kind == TypeKind::VECTOR)
			{
				VectorType* vector = (VectorType*)composite;
				return ConstantIndex(o[1], vector->count, "the component", &index) && Expect(type, vector->element, "the result");
			}
			if (composite->type_kind == TypeKind::MATRIX)
			{
				MatrixType* matrix = (MatrixType*)composite;
				return ConstantIndex(o[1], matrix->columns, "the column", &index) &&
					   Expect(type, GetVectorType(*module.context, matrix->element, matrix->rows), "the result");
			}
			return Fail("can't extract from %s", TypeName(composite));
		}
		case IROp::EXTRACT_DYNAMIC:
			if (types(0)->type_kind != TypeKind::VECTOR)
				return Fail("can't extract from %s", TypeName(types(0)));
			if (!IsIntegerScalar(types(1)))
				return Fail("the index is %s", TypeName(types(1)));
			return Expect(type, ((VectorType*)types(0))->element, "the result");
		case IROp::SHUFFLE:
		{
			if (types(0)->type_kind != TypeKind::VECTOR || types(1)->type_kind != TypeKind::VECTOR ||
				ComponentType(types(0)) != ComponentType(types(1)))
				return Fail("can't shuffle %s and %s", TypeName(types(0)), TypeName(types(1)));
			uint32 limit = ComponentCount(types(0)) + ComponentCount(types(1));
			for (uint32 i = 2; i < o.count; ++i)
			{
				uint32 index = 0;
				if (!ConstantIndex(o[i], limit, "the component", &index))
					return false;
			}
			return Expect(type, GetVectorType(*module.context, ComponentType(types(0)), o.count - 2), "the result");
		}
		case IROp::ADD:
		case IROp::SUB:
		case IROp::MUL:
		case IROp::DIV:
		case IROp::REM:
			if (!IsNumeric(type) && !IsFloatMatrix(type))
				return Fail("can't %s %s", info.name, TypeName(type));
			return Expect(types(0), type, "the left operand") && Expect(types(1), type, "the right operand");
		case IROp::NEG:
			if ((!IsNumeric(type) || ComponentKind(type) == PrimitiveKind::UNSIGNED) && !IsFloatMatrix(type))
				return Fail("can't negate %s", TypeName(type));
			return Expect(types(0), type, "the operand");
		case IROp::AND:
		case IROp::OR:
		case IROp::XOR:
		case IROp::NOT:
			if (!IsInteger(type) && !IsBoolScalarOrVector(type))
				return Fail("can't %s %s", info.name, TypeName(type));
			for (uint32 i = 0; i < o.count; ++i)
			{
				if (!Expect(types(i), type, "an operand"))
					return false;
			}
			return true;
		case IROp::SHL:
		case IROp::SHR:
			if (!IsInteger(type))
				return Fail("can't shift %s", TypeName(type));
			return Expect(types(0), type, "the value") && Expect(types(1), type, "the shift");
		case IROp::EQ:
		case IROp::NE:
		case IROp::LT:
		case IROp::LE:
		case IROp::GT:
		case IROp::GE:
		{
			bool equality = value.op == IROp::EQ || value.op == IROp::NE;
			if (!(equality ? IsScalarOrVector(types(0)) : IsNumeric(types(0))))
				return Fail("can't compare %s", TypeName(types(0)));
			return Expect(types(1), types(0), "the right operand") && Expect(type, BoolLike(types(0)), "the result");
		}
		case IROp::SELECT:
			if (!Expect(types(1), type, "the value if true") || !Expect(types(2), type, "the value if false"))
				return false;
			if (types(0) != module.context->bool_type && !(IsScalarOrVector(type) && types(0) == BoolLike(type)))
				return Fail("the condition is %s", TypeName(types(0)));
			return true;
		case IROp::MATMUL:
		{
			Context& context = *module.context;
			Type* product = nullptr;
			if (IsFloatMatrix(types(0)))
			{
				MatrixType* left = (MatrixType*)types(0);
				if (IsFloatVector(types(1)) && ((VectorType*)types(1))->count == left->columns)
					product = GetVectorType(context, left->element, left->rows);
				else if (IsFloatMatrix(types(1)) && ((MatrixType*)types(1))->rows == left->columns)
					product = GetMatrixType(context, left->element, ((MatrixType*)types(1))->columns, left->rows);
			}
			else if (IsFloatVector(types(0)) && IsFloatMatrix(types(1)) &&
					 ((VectorType*)types(0))->count == ((MatrixType*)types(1))->rows)
				product = GetVectorType(context, context.float_type, ((MatrixType*)types(1))->columns);
			if (!product)
				return Fail("can't multiply %s by %s", TypeName(types(0)), TypeName(types(1)));
			return Expect(type, product, "the result");
		}
		case IROp::CONVERT:
		{
			bool convertible = IsScalarOrVector(types(0)) && IsScalarOrVector(type) &&
							   ComponentCount(types(0)) == ComponentCount(type) &&
							   (types(0)->type_kind == TypeKind::VECTOR) == (type->type_kind == TypeKind::VECTOR);
			if (!convertible)
				return Fail("can't convert %s to %s", TypeName(types(0)), TypeName(type));
			return true;
		}
		case IROp::BITCAST:
			if (!IsNumeric(types(0)) || !IsNumeric(type) || types(0)->size != type->size)
				return Fail("can't bitcast %s to %s", TypeName(types(0)), TypeName(type));
			return true;
		case IROp::INTRINSIC:
			switch ((IRIntrinsic)value.sub_op)
			{
			case IRIntrinsic::MIN:
			case IRIntrinsic::MAX:
				if (o.count != 2 || !IsNumeric(type))
					return Fail("%s of %u operands of %s", IRIntrinsicToString((IRIntrinsic)value.sub_op).CString(), o.count,
						TypeName(type));
				return Expect(types(0), type, "the left operand") && Expect(types(1), type, "the right operand");
			case IRIntrinsic::DOT:
				if (o.count != 2 || !IsFloatVector(types(0)))
					return Fail("dot of %u operands of %s", o.count, TypeName(types(0)));
				return Expect(types(1), types(0), "the right operand") &&
					   Expect(type, module.context->float_type, "the result");
			case IRIntrinsic::LENGTH:
			case IRIntrinsic::NORMALIZE:
			{
				bool length = (IRIntrinsic)value.sub_op == IRIntrinsic::LENGTH;
				if (o.count != 1 || !IsFloatVector(types(0)))
					return Fail("%s of %u operands of %s", length ? "length" : "normalize", o.count, TypeName(types(0)));
				return Expect(type, length ? module.context->float_type : types(0), "the result");
			}
			}
			return Fail("intrinsic %u", (uint32)value.sub_op);
		case IROp::CALL:
		{
			if (!types(0) || types(0)->type_kind != TypeKind::FUNCTION)
				return Fail("calls a function of type %s", TypeName(types(0)));
			FunctionType* callee = (FunctionType*)types(0);
			if (o.count - 1 != callee->parameters.count)
				return Fail("passes %u arguments for %u parameters", o.count - 1, callee->parameters.count);
			for (uint32 i = 1; i < o.count; ++i)
			{
				if (!Expect(types(i), callee->parameters[i - 1], "an argument"))
					return false;
			}
			Type* result = callee->return_type == module.context->void_type ? nullptr : callee->return_type;
			return Expect(type, result, "the result");
		}
		case IROp::SELECTION_MERGE:
		case IROp::LOOP_MERGE:
		{
			IRRef terminator = value.instruction.next;
			if (!terminator || module[terminator].instruction.next || !(GetIROpInfo(module[terminator].op).flags & IR_OP_TERMINATOR))
				return Fail("has to come right before the terminator");
			IROp branch = module[terminator].op;
			if (value.op == IROp::SELECTION_MERGE ? branch != IROp::BRANCH_IF : (branch != IROp::BRANCH && branch != IROp::BRANCH_IF))
				return Fail("comes before %s", GetIROpInfo(branch).name);
			uint32 header = block_index[value.parent] - 1;
			for (uint32 i = 0; i < o.count; ++i)
			{
				uint32 target = block_index[o[i]] - 1;
				bool continue_target = value.op == IROp::LOOP_MERGE && i == 1;
				if (continue_target ? target < header : target <= header)
					return Fail("names block%u, before its header", target);
			}
			if (value.op == IROp::LOOP_MERGE && o[0] == o[1])
				return Fail("the merge block is the continue target");
			return true;
		}
		case IROp::BRANCH: return true;
		case IROp::BRANCH_IF: return Expect(types(0), module.context->bool_type, "the condition");
		case IROp::RETURN:
			if (function_type->return_type == module.context->void_type)
				return o.count == 0 || Fail("returns a value from a function returning void");
			if (o.count == 0)
				return Fail("returns nothing, expected %s", TypeName(function_type->return_type));
			return Expect(types(0), function_type->return_type, "the value");
		case IROp::DISCARD:
		case IROp::UNREACHABLE: return true;
		default: return Fail("isn't validated");
		}
	}

	bool Function(IRRef ref)
	{
		function = ref;
		instruction = 0;
		IRValue& value = module[ref];
		if (!value.function.info)
			return Fail("function has no info");
		if (!value.type || value.type->type_kind != TypeKind::FUNCTION)
			return Fail("the function's type is %s", TypeName(value.type));
		FunctionType* type = (FunctionType*)value.type;
		if (!Lists(type, value.function.info) || !Successors())
			return false;
		Dominators();

		for (uint32 b = 0; b < blocks.size(); ++b)
		{
			block_number = b;
			instruction_number = 0;
			for (IRRef current = module[blocks[b]].block.first; current; current = module[current].instruction.next)
			{
				instruction = current;
				IRRef next = module[current].instruction.next;
				if (next && (GetIROpInfo(module[current].op).flags & IR_OP_TERMINATOR))
					return Fail("a terminator before the end of the block");
				if (!Instruction(current, type))
					return false;
				instruction_number++;
			}
		}
		instruction = 0;
		return true;
	}
};

}

ZTStringView ValidateIR(IRModule& module, Arena* arena)
{
	Validator validator = { .module = module, .arena = arena };
	validator.block_index.assign(module.count, 0);
	validator.position.assign(module.count, 0);
	if (!validator.Globals())
		return validator.error;

	uint32 seen = 0;
	for (IRRef function = module.first_function; function; function = module[function].function.info->next)
	{
		if (!validator.Valid(function) || module[function].kind != IRValueKind::FUNCTION || ++seen > module.count)
		{
			validator.function = 0;
			validator.Fail("the function list is broken");
			return validator.error;
		}
		if (!validator.Function(function))
			return validator.error;
	}
	return validator.error;
}

}
