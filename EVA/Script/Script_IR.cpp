#include <EVA/Script/Script_IR.hpp>
#include <string.h>

namespace EVA::Script
{

static const IROpInfo IR_OPS[] = {
	{ "none", 0, 0, 0 },

	{ "load", 1, 1, IR_OP_RESULT | IR_OP_READS_MEMORY },
	{ "store", 2, 2, IR_OP_WRITES_MEMORY },
	{ "access", 2, IR_VARIADIC, IR_OP_RESULT },
	{ "copy", 2, 2, IR_OP_READS_MEMORY | IR_OP_WRITES_MEMORY },

	{ "offset", 2, 2, IR_OP_RESULT | IR_OP_RESERVED },
	{ "ptr_to_int", 1, 1, IR_OP_RESULT | IR_OP_RESERVED },
	{ "int_to_ptr", 1, 1, IR_OP_RESULT | IR_OP_RESERVED },

	{ "construct", 1, IR_VARIADIC, IR_OP_RESULT },
	{ "extract", 2, 2, IR_OP_RESULT },
	{ "extract_dynamic", 2, 2, IR_OP_RESULT },
	{ "shuffle", 4, 6, IR_OP_RESULT },

	{ "add", 2, 2, IR_OP_RESULT },
	{ "sub", 2, 2, IR_OP_RESULT },
	{ "mul", 2, 2, IR_OP_RESULT },
	{ "div", 2, 2, IR_OP_RESULT },
	{ "rem", 2, 2, IR_OP_RESULT },
	{ "neg", 1, 1, IR_OP_RESULT },

	{ "and", 2, 2, IR_OP_RESULT },
	{ "or", 2, 2, IR_OP_RESULT },
	{ "xor", 2, 2, IR_OP_RESULT },
	{ "not", 1, 1, IR_OP_RESULT },
	{ "shl", 2, 2, IR_OP_RESULT },
	{ "shr", 2, 2, IR_OP_RESULT },

	{ "eq", 2, 2, IR_OP_RESULT },
	{ "ne", 2, 2, IR_OP_RESULT },
	{ "lt", 2, 2, IR_OP_RESULT },
	{ "le", 2, 2, IR_OP_RESULT },
	{ "gt", 2, 2, IR_OP_RESULT },
	{ "ge", 2, 2, IR_OP_RESULT },

	{ "select", 3, 3, IR_OP_RESULT },

	{ "matmul", 2, 2, IR_OP_RESULT },
	{ "scale", 2, 2, IR_OP_RESULT | IR_OP_RESERVED },
	{ "transpose", 1, 1, IR_OP_RESULT | IR_OP_RESERVED },

	{ "convert", 1, 1, IR_OP_RESULT },
	{ "bitcast", 1, 1, IR_OP_RESULT },

	{ "intrinsic", 1, IR_VARIADIC, IR_OP_RESULT },
	{ "call", 1, IR_VARIADIC, IR_OP_RESULT | IR_OP_READS_MEMORY | IR_OP_WRITES_MEMORY | IR_OP_SIDE_EFFECTS },

	{ "selection_merge", 1, 1, IR_OP_SIDE_EFFECTS },
	{ "loop_merge", 2, 2, IR_OP_SIDE_EFFECTS },

	{ "branch", 1, 1, IR_OP_TERMINATOR | IR_OP_SIDE_EFFECTS },
	{ "branch_if", 3, 3, IR_OP_TERMINATOR | IR_OP_SIDE_EFFECTS },
	{ "return", 0, 1, IR_OP_TERMINATOR | IR_OP_SIDE_EFFECTS },
	{ "discard", 0, 0, IR_OP_TERMINATOR | IR_OP_SIDE_EFFECTS },
	{ "unreachable", 0, 0, IR_OP_TERMINATOR | IR_OP_SIDE_EFFECTS },
};
static_assert(sizeof(IR_OPS) / sizeof(IR_OPS[0]) == (size_t)IROp::COUNT, "IR_OPS has to match IROp");

const IROpInfo& GetIROpInfo(IROp op)
{
	assert(op < IROp::COUNT);
	return IR_OPS[(uint32)op];
}

ZTStringView IRIntrinsicToString(IRIntrinsic intrinsic)
{
	switch (intrinsic)
	{
		case IRIntrinsic::MIN: return "min";
		case IRIntrinsic::MAX: return "max";
		case IRIntrinsic::DOT: return "dot";
		case IRIntrinsic::LENGTH: return "length";
		case IRIntrinsic::NORMALIZE: return "normalize";
	}
	return "?";
}

void InitIRModule(IRModule& module, Context* context, Arena* arena)
{
	module.context = context;
	module.arena = arena;
	IRValue* page = (IRValue*)arena->Allocate(IR_PAGE_SIZE * sizeof(IRValue), alignof(IRValue));
	memset(&page[0], 0, sizeof(IRValue)); // the unused 0
	module.pages.push_back(page);
	module.count = 1;
}

static IRRef NewValue(IRModule& module, IRValueKind kind, Type* type)
{
	IRRef ref = module.free_list;
	if (ref)
		module.free_list = module[ref].parent;
	else
	{
		if (module.count % IR_PAGE_SIZE == 0)
			module.pages.push_back((IRValue*)module.arena->Allocate(IR_PAGE_SIZE * sizeof(IRValue), alignof(IRValue)));
		ref = module.count++;
	}
	IRValue& value = module[ref];
	memset(&value, 0, sizeof(value));
	value.kind = kind;
	value.type = type;
	return ref;
}

static void FreeValue(IRModule& module, IRRef ref)
{
	IRValue& value = module[ref];
	memset(&value, 0, sizeof(value));
	value.kind = IRValueKind::FREE;
	value.parent = module.free_list;
	module.free_list = ref;
}

IRRef AddIRFunction(IRModule& module, Atom name, FunctionType* type, Node* declaration)
{
	IRRef function = NewValue(module, IRValueKind::FUNCTION, type);
	IRFunctionInfo* info = module.arena->New<IRFunctionInfo>();
	info->name = name;
	info->declaration = declaration;
	module[function].function.info = info;

	for (uint32 i = 0; i < type->parameters.count; ++i)
	{
		IRRef parameter = NewValue(module, IRValueKind::PARAMETER, type->parameters[i]);
		module[parameter].parent = function;
		module[parameter].parameter.index = i;
		if (info->last_parameter)
			module[info->last_parameter].parameter.next = parameter;
		else
			info->first_parameter = parameter;
		info->last_parameter = parameter;
	}
	info->parameter_count = type->parameters.count;

	if (module.last_function)
		module[module.last_function].function.info->next = function;
	else
		module.first_function = function;
	module.last_function = function;
	return function;
}

IRRef AddIRBlock(IRModule& module, IRRef function)
{
	IRRef block = NewValue(module, IRValueKind::BLOCK, nullptr);
	IRFunctionData& data = module[function].function;
	module[block].parent = function;
	module[block].block.prev = data.last_block;
	if (data.last_block)
		module[data.last_block].block.next = block;
	else
		data.first_block = block;
	data.last_block = block;
	return block;
}

IRRef AddIRLocal(IRModule& module, IRRef function, Type* type, Constant* initializer)
{
	IRRef local = NewValue(module, IRValueKind::LOCAL, GetPointerType(*module.context, AddressSpace::FUNCTION, type));
	IRFunctionInfo* info = module[function].function.info;
	module[local].parent = function;
	module[local].local.initializer = initializer;
	module[local].local.index = info->local_count++;
	if (info->last_local)
		module[info->last_local].local.next = local;
	else
		info->first_local = local;
	info->last_local = local;
	return local;
}

IRRef AddIRGlobal(IRModule& module, Atom name, AddressSpace space, Type* type, Constant* initializer)
{
	IRRef global = NewValue(module, IRValueKind::GLOBAL, GetPointerType(*module.context, space, type));
	module[global].global.name = name;
	module[global].global.initializer = initializer;
	if (module.last_global)
		module[module.last_global].global.next = global;
	else
		module.first_global = global;
	module.last_global = global;
	return global;
}

IRRef GetIRParameter(IRModule& module, IRRef function, uint32 index)
{
	IRRef parameter = module[function].function.info->first_parameter;
	while (parameter && module[parameter].parameter.index != index)
		parameter = module[parameter].parameter.next;
	return parameter;
}

static uint32 HashConstant(Constant* constant)
{
	// FNV-1a over the type's address and the bytes.
	uint32 hash = 2166136261u;
	uint64 type = (uint64)(uintptr_t)constant->type;
	for (uint32 i = 0; i < 8; ++i)
		hash = (hash ^ (uint8)(type >> (i * 8))) * 16777619u;
	for (uint32 i = 0; i < constant->bytes.count; ++i)
		hash = (hash ^ constant->bytes.data[i]) * 16777619u;
	return hash;
}

IRRef GetIRConstant(IRModule& module, Constant* constant)
{
	IRRef* bucket = &module.constant_buckets[HashConstant(constant) % IR_CONSTANT_BUCKETS];
	for (IRRef ref = *bucket; ref; ref = module[ref].constant.next)
	{
		Constant* other = module[ref].constant.constant;
		if (other->type == constant->type && other->bytes.count == constant->bytes.count &&
			(!other->bytes.count || memcmp(other->bytes.data, constant->bytes.data, other->bytes.count) == 0))
			return ref;
	}
	IRRef ref = NewValue(module, IRValueKind::CONSTANT, constant->type);
	module[ref].constant.constant = constant;
	module[ref].constant.next = *bucket;
	*bucket = ref;
	return ref;
}

static IRRef GetIRScalar(IRModule& module, Type* type, uint32 bits)
{
	Constant constant;
	constant.type = type;
	constant.bytes = Slice<uint8>((uint8*)&bits, 4);
	IRRef* bucket = &module.constant_buckets[HashConstant(&constant) % IR_CONSTANT_BUCKETS];
	for (IRRef ref = *bucket; ref; ref = module[ref].constant.next)
	{
		Constant* other = module[ref].constant.constant;
		if (other->type == type && memcmp(other->bytes.data, &bits, 4) == 0)
			return ref;
	}
	Constant* copy = module.arena->New<Constant>();
	copy->type = type;
	uint8* bytes = (uint8*)module.arena->Allocate(4, 4);
	memcpy(bytes, &bits, 4);
	copy->bytes = Slice<uint8>(bytes, 4);
	return GetIRConstant(module, copy);
}

IRRef GetIRUint(IRModule& module, uint32 value)
{
	return GetIRScalar(module, module.context->uint_type, value);
}

IRRef GetIRInt(IRModule& module, int32 value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	return GetIRScalar(module, module.context->int_type, bits);
}

IRRef GetIRFloat(IRModule& module, float value)
{
	return GetIRScalar(module, module.context->float_type, FloatToBits(value));
}

static uint32 AppendOperands(IRModule& module, Slice<IRRef> operands)
{
	assert(operands.count <= 0xFFFF);
	std::vector<IRRef>& all = module.operands;
	uint32 offset = (uint32)all.size();
	// The operands can be another instruction's, which growing the array would move.
	bool inside = operands.count && operands.data >= all.data() && operands.data < all.data() + all.size();
	size_t start = inside ? (size_t)(operands.data - all.data()) : 0;
	for (uint32 i = 0; i < operands.count; ++i)
		all.push_back(inside ? all[start + i] : operands.data[i]);
	return offset;
}

static IRRef NewInstruction(IRModule& module, IRRef block, IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op)
{
	uint32 offset = AppendOperands(module, operands);
	IRRef instruction = NewValue(module, IRValueKind::INSTRUCTION, type);
	IRValue& value = module[instruction];
	value.op = op;
	value.sub_op = sub_op;
	value.parent = block;
	value.instruction.operands = offset;
	value.instruction.operand_count = (uint16)operands.count;
	return instruction;
}

IRRef AddIRInstruction(IRModule& module, IRRef block, IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op)
{
	IRRef instruction = NewInstruction(module, block, op, type, operands, sub_op);
	IRBlockData& data = module[block].block;
	module[instruction].instruction.prev = data.last;
	if (data.last)
		module[data.last].instruction.next = instruction;
	else
		data.first = instruction;
	data.last = instruction;
	return instruction;
}

IRRef InsertIRInstruction(IRModule& module, IRRef before, IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op)
{
	IRRef block = module[before].parent;
	IRRef instruction = NewInstruction(module, block, op, type, operands, sub_op);
	IRRef prev = module[before].instruction.prev;
	module[instruction].instruction.prev = prev;
	module[instruction].instruction.next = before;
	module[before].instruction.prev = instruction;
	if (prev)
		module[prev].instruction.next = instruction;
	else
		module[block].block.first = instruction;
	return instruction;
}

void RemoveIRInstruction(IRModule& module, IRRef instruction)
{
	IRValue& value = module[instruction];
	IRBlockData& block = module[value.parent].block;
	IRRef prev = value.instruction.prev;
	IRRef next = value.instruction.next;
	if (prev)
		module[prev].instruction.next = next;
	else
		block.first = next;
	if (next)
		module[next].instruction.prev = prev;
	else
		block.last = prev;
	FreeValue(module, instruction);
}

Slice<IRRef> GetIROperands(IRModule& module, IRRef instruction)
{
	IRInstructionData& data = module[instruction].instruction;
	return Slice<IRRef>(module.operands.data() + data.operands, data.operand_count);
}

void FindReachable(IRModule& module, IRRef wrapper, IRReachable& out)
{
	out.functions.clear();
	out.globals.clear();
	std::vector<uint8> seen(module.count, 0);

	// Depth first without recursion, so a long chain of calls can't overflow the stack. A function is added once all its
	// callees are.
	struct Frame
	{
		IRRef function;
		std::vector<IRRef> callees;
		size_t next;
	};
	std::vector<Frame> stack;
	auto visit = [&](IRRef function) {
		seen[function] = 1;
		Frame frame = { .function = function, .next = 0 };
		for (IRRef block = module[function].function.first_block; block; block = module[block].block.next)
		{
			for (IRRef instruction = module[block].block.first; instruction; instruction = module[instruction].instruction.next)
			{
				Slice<IRRef> operands = GetIROperands(module, instruction);
				for (uint32 i = 0; i < operands.count; ++i)
				{
					IRValueKind kind = module[operands[i]].kind;
					if (kind == IRValueKind::GLOBAL)
						seen[operands[i]] = 1;
					else if (kind == IRValueKind::FUNCTION)
						frame.callees.push_back(operands[i]);
				}
			}
		}
		stack.push_back(std::move(frame));
	};
	visit(wrapper);
	while (!stack.empty())
	{
		Frame& frame = stack.back();
		if (frame.next < frame.callees.size())
		{
			IRRef callee = frame.callees[frame.next++];
			if (!seen[callee])
				visit(callee);
			continue;
		}
		out.functions.push_back(frame.function);
		stack.pop_back();
	}

	for (IRRef global = module.first_global; global; global = module[global].global.next)
	{
		if (seen[global])
			out.globals.push_back(global);
	}
}

// Dumping

namespace
{

struct Printer
{
	IRModule& module;
	StringBuilder& builder;
	std::vector<uint32> numbers; // per value: %N, $N or blockN within its function

	bool Valid(IRRef ref) { return ref && ref < module.count && module[ref].kind != IRValueKind::FREE; }

	const char* Name(Atom atom) { return GetAtomString(atom, builder.arena).CString(); }

	void PrintType(Type* type)
	{
		builder.Append(type ? TypeToString(type, builder.arena) : ZTStringView("?"));
	}

	void PrintOperand(IRRef ref)
	{
		if (!Valid(ref))
		{
			builder.AppendFormat("<invalid %u>", ref);
			return;
		}
		IRValue& value = module[ref];
		switch (value.kind)
		{
		case IRValueKind::INSTRUCTION:
		case IRValueKind::PARAMETER: builder.AppendFormat("%%%u", numbers[ref]); break;
		case IRValueKind::LOCAL: builder.AppendFormat("$%u", value.local.index); break;
		case IRValueKind::BLOCK: builder.AppendFormat("block%u", numbers[ref]); break;
		case IRValueKind::GLOBAL: builder.AppendFormat("@%s", Name(value.global.name)); break;
		case IRValueKind::FUNCTION: builder.AppendFormat("@%s", Name(value.function.info->name)); break;
		case IRValueKind::CONSTANT:
			PrintType(value.type);
			builder.Append(" ");
			builder.Append(ConstantToString(value.constant.constant, builder.arena));
			break;
		case IRValueKind::FREE: break;
		}
	}

	void PrintGlobal(IRRef ref)
	{
		IRValue& value = module[ref];
		builder.AppendFormat("global @%s: ", Name(value.global.name));
		PrintType(value.type);
		PointerType* pointer = value.type && value.type->type_kind == TypeKind::POINTER ? (PointerType*)value.type : nullptr;
		bool interface = pointer && (pointer->space == AddressSpace::INPUT || pointer->space == AddressSpace::OUTPUT);
		if (interface && value.global.io)
		{
			ShaderIO* io = value.global.io;
			if (io->io_kind == IOKind::SEMANTIC)
				builder.AppendFormat(" semantic(%s)", SemanticToString(io->semantic).CString());
			else
				builder.AppendFormat(" location(%u)", io->location);
		}
		else if (pointer && pointer->space == AddressSpace::UNIFORM && value.global.bind_group)
			builder.AppendFormat(" bind_group(%u)", value.global.bind_group->reflection.index);
		else if (!interface && value.global.initializer)
		{
			builder.Append(" = ");
			builder.Append(ConstantToString(value.global.initializer, builder.arena));
		}
		builder.Append("\n");
	}

	void PrintInstruction(IRRef ref)
	{
		IRValue& value = module[ref];
		builder.Append("\t");
		if (value.type)
		{
			builder.AppendFormat("%%%u: ", numbers[ref]);
			PrintType(value.type);
			builder.Append(" = ");
		}
		builder.Append(value.op < IROp::COUNT ? GetIROpInfo(value.op).name : "?");
		if (value.op == IROp::INTRINSIC)
			builder.AppendFormat(" %s", IRIntrinsicToString((IRIntrinsic)value.sub_op).CString());
		Slice<IRRef> operands = GetIROperands(module, ref);
		for (uint32 i = 0; i < operands.count; ++i)
		{
			builder.Append(i ? ", " : " ");
			PrintOperand(operands[i]);
		}
		builder.Append("\n");
	}

	void PrintFunction(IRRef ref)
	{
		IRValue& value = module[ref];
		IRFunctionInfo* info = value.function.info;

		// Number the parameters and results, then the blocks, in order.
		uint32 next_value = 0;
		for (IRRef parameter = info->first_parameter; Valid(parameter); parameter = module[parameter].parameter.next)
			numbers[parameter] = next_value++;
		uint32 next_block = 0;
		for (IRRef block = value.function.first_block; Valid(block); block = module[block].block.next)
		{
			numbers[block] = next_block++;
			for (IRRef instruction = module[block].block.first; Valid(instruction); instruction = module[instruction].instruction.next)
			{
				if (module[instruction].type)
					numbers[instruction] = next_value++;
			}
		}

		builder.AppendFormat("function @%s(", Name(info->name));
		for (IRRef parameter = info->first_parameter; Valid(parameter); parameter = module[parameter].parameter.next)
		{
			if (parameter != info->first_parameter)
				builder.Append(", ");
			builder.AppendFormat("%%%u: ", numbers[parameter]);
			PrintType(module[parameter].type);
		}
		builder.Append("): ");
		bool function_type = value.type && value.type->type_kind == TypeKind::FUNCTION;
		PrintType(function_type ? ((FunctionType*)value.type)->return_type : nullptr);
		if (info->entry_point)
			builder.AppendFormat(" [entry %s]", ShaderStageToString(info->entry_point->stage).CString());
		builder.Append("\n");

		for (IRRef local = info->first_local; Valid(local); local = module[local].local.next)
		{
			builder.AppendFormat("\tlocal $%u: ", module[local].local.index);
			PrintType(module[local].type);
			if (module[local].local.initializer)
			{
				builder.Append(" = ");
				builder.Append(ConstantToString(module[local].local.initializer, builder.arena));
			}
			builder.Append("\n");
		}

		for (IRRef block = value.function.first_block; Valid(block); block = module[block].block.next)
		{
			builder.AppendFormat("block%u:\n", numbers[block]);
			for (IRRef instruction = module[block].block.first; Valid(instruction); instruction = module[instruction].instruction.next)
				PrintInstruction(instruction);
		}
	}
};

}

ZTStringView IRModuleToString(IRModule& module, Arena* arena)
{
	StringBuilder builder(arena);
	Printer printer = { .module = module, .builder = builder };
	printer.numbers.assign(module.count, 0);

	for (IRRef global = module.first_global; printer.Valid(global); global = module[global].global.next)
		printer.PrintGlobal(global);
	for (IRRef function = module.first_function; printer.Valid(function); function = module[function].function.info->next)
	{
		if (function != module.first_function || module.first_global)
			builder.Append("\n");
		printer.PrintFunction(function);
	}
	return builder.ToString();
}

}
