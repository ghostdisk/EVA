#include <EVA/Script/Script_IR.hpp>

// The safety pass: see Docs/Plan/Shaders.md (11). For now only the index clamps, which are also what keeps fxc from
// rejecting an index it can fold to an out of bounds constant.

namespace EVA::Script
{

// min(uint(index), limit - 1), inserted before the instruction using it.
static IRRef Clamp(IRModule& module, IRRef before, IRRef index, uint32 limit)
{
	Context& context = *module.context;
	if (module[index].type != context.uint_type)
		index = InsertIRInstruction(module, before, IROp::CONVERT, context.uint_type, { index });
	return InsertIRInstruction(module, before, IROp::INTRINSIC, context.uint_type, { index, GetIRUint(module, limit - 1) },
		(uint8)IRIntrinsic::MIN);
}

// Replaces operand i of the instruction, unless it's a constant.
static void ClampOperand(IRModule& module, IRRef instruction, uint32 i, uint32 limit)
{
	// Inserting adds operands to the module, which can move them, so they're looked up again after.
	IRRef index = GetIROperands(module, instruction)[i];
	if (module[index].kind == IRValueKind::CONSTANT)
		return;
	IRRef clamped = Clamp(module, instruction, index, limit);
	module.operands[module[instruction].instruction.operands + i] = clamped;
}

void ClampIndices(IRModule& module)
{
	Context& context = *module.context;
	for (IRRef function = module.first_function; function; function = module[function].function.info->next)
	{
		for (IRRef block = module[function].function.first_block; block; block = module[block].block.next)
		{
			for (IRRef instruction = module[block].block.first; instruction; instruction = module[instruction].instruction.next)
			{
				IROp op = module[instruction].op;
				if (op == IROp::EXTRACT_DYNAMIC)
				{
					IRRef vector = GetIROperands(module, instruction)[0];
					ClampOperand(module, instruction, 1, ((VectorType*)module[vector].type)->count);
					continue;
				}
				if (op != IROp::ACCESS)
					continue;

				Slice<IRRef> operands = GetIROperands(module, instruction);
				Type* current = ((PointerType*)module[operands[0]].type)->pointee;
				uint32 count = operands.count;
				for (uint32 i = 1; i < count; ++i)
				{
					switch (current->type_kind)
					{
					case TypeKind::STRUCT:
					{
						IRRef index = GetIROperands(module, instruction)[i];
						uint32 field;
						memcpy(&field, module[index].constant.constant->bytes.data, 4);
						current = ((StructType*)current)->fields[field].type;
						break;
					}
					case TypeKind::ARRAY:
						ClampOperand(module, instruction, i, ((ArrayType*)current)->length);
						current = ((ArrayType*)current)->element;
						break;
					case TypeKind::VECTOR:
						ClampOperand(module, instruction, i, ((VectorType*)current)->count);
						current = ((VectorType*)current)->element;
						break;
					case TypeKind::MATRIX:
					{
						MatrixType* matrix = (MatrixType*)current;
						ClampOperand(module, instruction, i, matrix->columns);
						current = GetVectorType(context, matrix->element, matrix->rows);
						break;
					}
					default: break;
					}
				}
			}
		}
	}
}

}
