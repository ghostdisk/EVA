#include <EVA/Script/Script_IR.hpp>
#include <EVA/Core/Panic.hpp>
#include <map>
#include <unordered_map>

// Emits one entry point as a SPIR-V 1.0 module for Vulkan, shaped like glslang's output, which is what drivers are
// tested on. The IR maps almost one to one: its control flow is already structured, its locals are Function variables,
// its interface globals Input and Output variables. Only blocks reachable from the entry block are emitted; merge blocks
// nothing reaches get a bare OpUnreachable.
//
// Vulkan's clip space has Y pointing down, D3D's and Metal's up, so the vertex wrapper negates the Y of the position it
// outputs. Every pixel then lands in the same row on every API, render targets included; user code never sees it.

namespace EVA::Script
{

namespace
{

// The opcodes and enumerants used, from the SPIR-V specification.
enum SpvOp : uint16
{
	OpExtInstImport = 11,
	OpExtInst = 12,
	OpMemoryModel = 14,
	OpEntryPoint = 15,
	OpExecutionMode = 16,
	OpCapability = 17,
	OpTypeVoid = 19,
	OpTypeBool = 20,
	OpTypeInt = 21,
	OpTypeFloat = 22,
	OpTypeVector = 23,
	OpTypeMatrix = 24,
	OpTypeArray = 28,
	OpTypeStruct = 30,
	OpTypePointer = 32,
	OpTypeFunction = 33,
	OpConstantTrue = 41,
	OpConstantFalse = 42,
	OpConstant = 43,
	OpConstantComposite = 44,
	OpConstantNull = 46,
	OpFunction = 54,
	OpFunctionParameter = 55,
	OpFunctionEnd = 56,
	OpFunctionCall = 57,
	OpVariable = 59,
	OpLoad = 61,
	OpStore = 62,
	OpAccessChain = 65,
	OpDecorate = 71,
	OpVectorExtractDynamic = 77,
	OpVectorShuffle = 79,
	OpCompositeConstruct = 80,
	OpCompositeExtract = 81,
	OpCompositeInsert = 82,
	OpConvertFToU = 109,
	OpConvertFToS = 110,
	OpConvertSToF = 111,
	OpConvertUToF = 112,
	OpBitcast = 124,
	OpSNegate = 126,
	OpFNegate = 127,
	OpIAdd = 128,
	OpFAdd = 129,
	OpISub = 130,
	OpFSub = 131,
	OpIMul = 132,
	OpFMul = 133,
	OpUDiv = 134,
	OpSDiv = 135,
	OpFDiv = 136,
	OpUMod = 137,
	OpFRem = 140,
	OpVectorTimesMatrix = 144,
	OpMatrixTimesVector = 145,
	OpMatrixTimesMatrix = 146,
	OpDot = 148,
	OpLogicalEqual = 164,
	OpLogicalNotEqual = 165,
	OpLogicalOr = 166,
	OpLogicalAnd = 167,
	OpLogicalNot = 168,
	OpSelect = 169,
	OpIEqual = 170,
	OpINotEqual = 171,
	OpUGreaterThan = 172,
	OpSGreaterThan = 173,
	OpUGreaterThanEqual = 174,
	OpSGreaterThanEqual = 175,
	OpULessThan = 176,
	OpSLessThan = 177,
	OpULessThanEqual = 178,
	OpSLessThanEqual = 179,
	OpFOrdEqual = 180,
	OpFUnordNotEqual = 183,
	OpFOrdLessThan = 184,
	OpFOrdGreaterThan = 186,
	OpFOrdLessThanEqual = 188,
	OpFOrdGreaterThanEqual = 190,
	OpShiftRightLogical = 194,
	OpShiftRightArithmetic = 195,
	OpShiftLeftLogical = 196,
	OpBitwiseOr = 197,
	OpBitwiseXor = 198,
	OpBitwiseAnd = 199,
	OpNot = 200,
	OpLoopMerge = 246,
	OpSelectionMerge = 247,
	OpLabel = 248,
	OpBranch = 249,
	OpBranchConditional = 250,
	OpKill = 252,
	OpReturn = 253,
	OpReturnValue = 254,
	OpUnreachable = 255,
};

static const uint32 SPIRV_MAGIC = 0x07230203;
static const uint32 SPIRV_VERSION_1_0 = 0x00010000;
static const uint32 CAPABILITY_SHADER = 1;
static const uint32 ADDRESSING_LOGICAL = 0;
static const uint32 MEMORY_MODEL_GLSL450 = 1;
static const uint32 EXECUTION_MODEL_VERTEX = 0;
static const uint32 EXECUTION_MODEL_FRAGMENT = 4;
static const uint32 EXECUTION_MODE_ORIGIN_UPPER_LEFT = 7;
static const uint32 STORAGE_INPUT = 1;
static const uint32 STORAGE_OUTPUT = 3;
static const uint32 STORAGE_PRIVATE = 6;
static const uint32 STORAGE_FUNCTION = 7;
static const uint32 DECORATION_BUILT_IN = 11;
static const uint32 DECORATION_FLAT = 14;
static const uint32 DECORATION_LOCATION = 30;
static const uint32 BUILT_IN_POSITION = 0;
static const uint32 BUILT_IN_FRAG_COORD = 15;
static const uint32 BUILT_IN_VERTEX_INDEX = 42;

// GLSL.std.450 extended instructions.
static const uint32 GLSL_FMIN = 37;
static const uint32 GLSL_UMIN = 38;
static const uint32 GLSL_SMIN = 39;
static const uint32 GLSL_FMAX = 40;
static const uint32 GLSL_UMAX = 41;
static const uint32 GLSL_SMAX = 42;
static const uint32 GLSL_LENGTH = 66;
static const uint32 GLSL_NORMALIZE = 69;

// An instruction's word count is 16 bits.
static const uint32 MAX_INSTRUCTION_WORDS = 0xFFFF;

typedef std::vector<uint32> Words;

// A constant's type and bytes, which live in arenas for as long as the module is emitted.
struct ConstantKey
{
	Type* type;
	const uint8* bytes;

	bool operator==(const ConstantKey& other) const
	{
		return type == other.type && memcmp(bytes, other.bytes, type->size) == 0;
	}
};

struct ConstantKeyHash
{
	size_t operator()(const ConstantKey& key) const
	{
		// FNV-1a over the bytes, mixed with the type's address.
		uint64 hash = 14695981039346656037ull ^ (uint64)(uintptr_t)key.type;
		for (uint32 i = 0; i < key.type->size; ++i)
			hash = (hash ^ key.bytes[i]) * 1099511628211ull;
		return (size_t)hash;
	}
};

uint32 GetStorageClass(AddressSpace space)
{
	switch (space)
	{
	case AddressSpace::FUNCTION: return STORAGE_FUNCTION;
	case AddressSpace::PRIVATE:
	case AddressSpace::CONSTANT: return STORAGE_PRIVATE;
	case AddressSpace::INPUT: return STORAGE_INPUT;
	case AddressSpace::OUTPUT: return STORAGE_OUTPUT;
	case AddressSpace::MEMORY: break;
	}
	Panic("SPIR-V: no storage class for %s", AddressSpaceToString(space).CString());
}

struct Emitter
{
	IRModule& module;
	Context& context;
	uint32 next_id = 1;

	// The module's sections, in the order the specification requires.
	Words imports;
	Words entry_points;
	Words execution_modes;
	Words annotations;
	Words declarations; // types, constants and global variables
	Words functions;
	uint32 glsl_std = 0; // the GLSL.std.450 import, once used

	std::unordered_map<Type*, uint32> types;
	std::map<std::pair<uint32, uint32>, uint32> pointer_types; // storage class, pointee: constant and private share one
	std::map<Words, uint32> function_types;
	std::unordered_map<ConstantKey, uint32, ConstantKeyHash> constants;
	std::unordered_map<uint32, uint32> nulls;          // type id to OpConstantNull
	std::vector<uint32> ids;                           // per IRRef

	// For the function being emitted.
	Words* out = nullptr;
	std::vector<uint8> reachable;      // per IRRef of its blocks
	std::vector<IRRef> continue_owner; // per IRRef of a continue target: its loop header

	size_t too_large = 0; // the word count of the first instruction too long for SPIR-V, if any

	uint32 NewId() { return next_id++; }

	void Emit(Words& words, SpvOp op, std::initializer_list<uint32> operands, const Words& extra = {})
	{
		size_t count = 1 + operands.size() + extra.size();
		if (count > MAX_INSTRUCTION_WORDS)
		{
			// A huge constant array or struct. The module is thrown away, so the instruction is just left out.
			if (!too_large)
				too_large = count;
			return;
		}
		words.push_back((uint32)count << 16 | op);
		words.insert(words.end(), operands.begin(), operands.end());
		words.insert(words.end(), extra.begin(), extra.end());
	}

	// A nul-terminated string as words.
	static Words String(const char* text)
	{
		Words words;
		size_t length = strlen(text) + 1;
		words.assign((length + 3) / 4, 0);
		memcpy(words.data(), text, length);
		return words;
	}

	uint32 GlslStd()
	{
		if (!glsl_std)
		{
			glsl_std = NewId();
			Emit(imports, OpExtInstImport, { glsl_std }, String("GLSL.std.450"));
		}
		return glsl_std;
	}

	// Types

	uint32 PointerTypeId(uint32 storage_class, uint32 pointee)
	{
		auto found = pointer_types.find({ storage_class, pointee });
		if (found != pointer_types.end())
			return found->second;
		uint32 id = NewId();
		Emit(declarations, OpTypePointer, { id, storage_class, pointee });
		pointer_types[{ storage_class, pointee }] = id;
		return id;
	}

	uint32 TypeId(Type* type)
	{
		auto found = types.find(type);
		if (found != types.end())
			return found->second;

		uint32 id = 0;
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE:
			id = NewId();
			switch (((PrimitiveType*)type)->primitive_kind)
			{
			case PrimitiveKind::VOID: Emit(declarations, OpTypeVoid, { id }); break;
			case PrimitiveKind::BOOL: Emit(declarations, OpTypeBool, { id }); break;
			case PrimitiveKind::SIGNED: Emit(declarations, OpTypeInt, { id, 32, 1 }); break;
			case PrimitiveKind::UNSIGNED: Emit(declarations, OpTypeInt, { id, 32, 0 }); break;
			case PrimitiveKind::FLOAT: Emit(declarations, OpTypeFloat, { id, 32 }); break;
			}
			break;
		case TypeKind::VECTOR:
		{
			VectorType* vector = (VectorType*)type;
			uint32 element = TypeId(vector->element);
			id = NewId();
			Emit(declarations, OpTypeVector, { id, element, vector->count });
			break;
		}
		case TypeKind::MATRIX:
		{
			MatrixType* matrix = (MatrixType*)type;
			uint32 column = TypeId(GetVectorType(context, matrix->element, matrix->rows));
			id = NewId();
			Emit(declarations, OpTypeMatrix, { id, column, matrix->columns });
			break;
		}
		case TypeKind::ARRAY:
		{
			ArrayType* array = (ArrayType*)type;
			uint32 element = TypeId(array->element);
			uint32 length = UintConstant(array->length);
			id = NewId();
			Emit(declarations, OpTypeArray, { id, element, length });
			break;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			Words members;
			for (uint32 i = 0; i < structure->fields.count; ++i)
				members.push_back(TypeId(structure->fields[i].type));
			id = NewId();
			Emit(declarations, OpTypeStruct, { id }, members);
			break;
		}
		case TypeKind::POINTER:
		{
			PointerType* pointer = (PointerType*)type;
			id = PointerTypeId(GetStorageClass(pointer->space), TypeId(pointer->pointee));
			break;
		}
		case TypeKind::FUNCTION:
		{
			FunctionType* function = (FunctionType*)type;
			Words signature = { TypeId(function->return_type) };
			for (uint32 i = 0; i < function->parameters.count; ++i)
				signature.push_back(TypeId(function->parameters[i]));
			auto existing = function_types.find(signature);
			if (existing != function_types.end())
			{
				id = existing->second;
				break;
			}
			id = NewId();
			Emit(declarations, OpTypeFunction, { id }, signature);
			function_types[signature] = id;
			break;
		}
		case TypeKind::ENUM: Panic("SPIR-V: enum types aren't values");
		}
		types[type] = id;
		return id;
	}

	// Constants

	// A constant of type from its bytes, laid out by the type. Elements are emitted before the composites using them.
	uint32 ConstantId(Type* type, const uint8* bytes)
	{
		ConstantKey key = { type, bytes };
		auto found = constants.find(key);
		if (found != constants.end())
			return found->second;

		uint32 type_id = TypeId(type);
		uint32 id = 0;
		Words parts;
		switch (type->type_kind)
		{
		case TypeKind::PRIMITIVE:
		{
			uint32 bits;
			memcpy(&bits, bytes, 4);
			id = NewId();
			if (((PrimitiveType*)type)->primitive_kind == PrimitiveKind::BOOL)
				Emit(declarations, bits ? OpConstantTrue : OpConstantFalse, { type_id, id });
			else
				Emit(declarations, OpConstant, { type_id, id, bits });
			break;
		}
		case TypeKind::VECTOR:
		{
			VectorType* vector = (VectorType*)type;
			for (uint32 i = 0; i < vector->count; ++i)
				parts.push_back(ConstantId(vector->element, bytes + i * vector->element->size));
			break;
		}
		case TypeKind::MATRIX:
		{
			MatrixType* matrix = (MatrixType*)type;
			Type* column = GetVectorType(context, matrix->element, matrix->rows);
			for (uint32 i = 0; i < matrix->columns; ++i)
				parts.push_back(ConstantId(column, bytes + i * column->size));
			break;
		}
		case TypeKind::ARRAY:
		{
			ArrayType* array = (ArrayType*)type;
			for (uint32 i = 0; i < array->length; ++i)
				parts.push_back(ConstantId(array->element, bytes + (size_t)i * array->stride));
			break;
		}
		case TypeKind::STRUCT:
		{
			StructType* structure = (StructType*)type;
			for (uint32 i = 0; i < structure->fields.count; ++i)
				parts.push_back(ConstantId(structure->fields[i].type, bytes + structure->fields[i].offset));
			break;
		}
		default: Panic("SPIR-V: no constants of %s", TypeToString(type, module.arena).CString());
		}
		if (!id)
		{
			id = NewId();
			Emit(declarations, OpConstantComposite, { type_id, id }, parts);
		}
		// The bytes can be the caller's temporaries, so the key gets its own copy.
		uint8* copy = (uint8*)module.arena->Allocate(type->size, 4);
		memcpy(copy, bytes, type->size);
		constants[{ type, copy }] = id;
		return id;
	}

	uint32 UintConstant(uint32 value) { return ConstantId(context.uint_type, (const uint8*)&value); }

	// Every component of a scalar or vector type set to value, given as the component's bits.
	uint32 Splat(Type* type, uint32 bits)
	{
		uint32 bytes[4] = { bits, bits, bits, bits };
		return ConstantId(type, (const uint8*)bytes);
	}

	uint32 Null(Type* type)
	{
		uint32 type_id = TypeId(type);
		auto found = nulls.find(type_id);
		if (found != nulls.end())
			return found->second;
		uint32 id = NewId();
		Emit(declarations, OpConstantNull, { type_id, id });
		nulls[type_id] = id;
		return id;
	}

	// Values

	uint32 Id(IRRef ref)
	{
		IRValue& value = module[ref];
		if (value.kind == IRValueKind::CONSTANT && !ids[ref])
			ids[ref] = ConstantId(value.type, value.constant.constant->bytes.data);
		assert(ids[ref]);
		return ids[ref];
	}

	// A constant index operand's value.
	uint32 Literal(IRRef ref)
	{
		uint32 value;
		memcpy(&value, module[ref].constant.constant->bytes.data, 4);
		return value;
	}

	// An instruction producing a value of type, which becomes ref's id.
	uint32 Result(IRRef ref, SpvOp op, Type* type, std::initializer_list<uint32> operands, const Words& extra = {})
	{
		uint32 id = Value(op, type, operands, extra);
		ids[ref] = id;
		return id;
	}

	uint32 Value(SpvOp op, Type* type, std::initializer_list<uint32> operands, const Words& extra = {})
	{
		uint32 type_id = TypeId(type);
		uint32 id = NewId();
		Words all(operands);
		all.insert(all.end(), extra.begin(), extra.end());
		Emit(*out, op, { type_id, id }, all);
		return id;
	}

	// The same op on each column of matrices, which SPIR-V's arithmetic doesn't take.
	uint32 Columns(SpvOp op, MatrixType* matrix, Slice<uint32> operands)
	{
		Type* column = GetVectorType(context, matrix->element, matrix->rows);
		Words columns;
		for (uint32 c = 0; c < matrix->columns; ++c)
		{
			Words parts;
			for (uint32 i = 0; i < operands.count; ++i)
				parts.push_back(Value(OpCompositeExtract, column, { operands[i], c }));
			columns.push_back(Value(op, column, {}, parts));
		}
		return Value(OpCompositeConstruct, matrix, {}, columns);
	}

	// Arithmetic on ints, uints, floats or float matrices: the op for each kind.
	uint32 Arithmetic(IRRef ref, SpvOp signed_op, SpvOp unsigned_op, SpvOp float_op, Slice<IRRef> o)
	{
		Type* type = module[ref].type;
		Words operands;
		for (uint32 i = 0; i < o.count; ++i)
			operands.push_back(Id(o[i]));
		if (type->type_kind == TypeKind::MATRIX)
			return ids[ref] = Columns(float_op, (MatrixType*)type, Slice<uint32>(operands.data(), (uint32)operands.size()));
		PrimitiveKind kind = GetScalarKind(type);
		SpvOp op = kind == PrimitiveKind::FLOAT ? float_op : kind == PrimitiveKind::SIGNED ? signed_op : unsigned_op;
		return Result(ref, op, type, {}, operands);
	}

	uint32 Compare(IRRef ref, SpvOp signed_op, SpvOp unsigned_op, SpvOp float_op, SpvOp bool_op, Slice<IRRef> o)
	{
		SpvOp op = signed_op;
		switch (GetScalarKind(module[o[0]].type))
		{
		case PrimitiveKind::SIGNED: op = signed_op; break;
		case PrimitiveKind::UNSIGNED: op = unsigned_op; break;
		case PrimitiveKind::FLOAT: op = float_op; break;
		case PrimitiveKind::BOOL: op = bool_op; break;
		case PrimitiveKind::VOID: Panic("SPIR-V: comparing void");
		}
		return Result(ref, op, module[ref].type, { Id(o[0]), Id(o[1]) });
	}

	void Convert(IRRef ref, IRRef operand)
	{
		Type* to = module[ref].type;
		Type* from = module[operand].type;
		if (to == from)
		{
			ids[ref] = Id(operand);
			return;
		}
		PrimitiveKind to_kind = GetScalarKind(to);
		PrimitiveKind from_kind = GetScalarKind(from);
		uint32 value = Id(operand);
		if (to_kind == PrimitiveKind::BOOL)
		{
			Result(ref, from_kind == PrimitiveKind::FLOAT ? OpFUnordNotEqual : OpINotEqual, to, { value, Splat(from, 0) });
			return;
		}
		if (from_kind == PrimitiveKind::BOOL)
		{
			float one = 1.0f;
			uint32 one_bits = 1;
			if (to_kind == PrimitiveKind::FLOAT)
				memcpy(&one_bits, &one, 4);
			Result(ref, OpSelect, to, { value, Splat(to, one_bits), Splat(to, 0) });
			return;
		}
		SpvOp op = OpBitcast; // between int and uint
		if (from_kind == PrimitiveKind::FLOAT)
			op = to_kind == PrimitiveKind::SIGNED ? OpConvertFToS : OpConvertFToU;
		else if (to_kind == PrimitiveKind::FLOAT)
			op = from_kind == PrimitiveKind::SIGNED ? OpConvertSToF : OpConvertUToF;
		Result(ref, op, to, { value });
	}

	void Select(IRRef ref, Slice<IRRef> o)
	{
		Type* type = module[ref].type;
		uint32 condition = Id(o[0]);
		uint32 a = Id(o[1]);
		uint32 b = Id(o[2]);
		if (type->type_kind == TypeKind::MATRIX)
		{
			MatrixType* matrix = (MatrixType*)type;
			Type* column = GetVectorType(context, matrix->element, matrix->rows);
			Words columns;
			for (uint32 c = 0; c < matrix->columns; ++c)
			{
				uint32 x = Value(OpCompositeExtract, column, { a, c });
				uint32 y = Value(OpCompositeExtract, column, { b, c });
				columns.push_back(Value(OpSelect, column, { BoolCondition(condition, o[0], column), x, y }));
			}
			ids[ref] = Value(OpCompositeConstruct, type, {}, columns);
			return;
		}
		Result(ref, OpSelect, type, { BoolCondition(condition, o[0], type), a, b });
	}

	// SPIR-V 1.0's OpSelect needs as many condition components as the result has: a scalar condition is splat.
	uint32 BoolCondition(uint32 condition, IRRef condition_ref, Type* type)
	{
		if (type->type_kind != TypeKind::VECTOR || module[condition_ref].type->type_kind == TypeKind::VECTOR)
			return condition;
		uint32 count = ((VectorType*)type)->count;
		Words parts(count, condition);
		return Value(OpCompositeConstruct, GetVectorType(context, context.bool_type, count), {}, parts);
	}

	void Instruction(IRRef ref)
	{
		IRValue& value = module[ref];
		Slice<IRRef> o = GetIROperands(module, ref);
		Type* type = value.type;
		switch (value.op)
		{
		case IROp::LOAD: Result(ref, OpLoad, type, { Id(o[0]) }); break;
		case IROp::STORE:
		{
			uint32 stored = Id(o[1]);
			IRValue& pointer = module[o[0]];
			bool output = pointer.kind == IRValueKind::GLOBAL && ((PointerType*)pointer.type)->space == AddressSpace::OUTPUT;
			if (output && pointer.global.io->io_kind == IOKind::SEMANTIC && pointer.global.io->semantic == Semantic::POSITION)
			{
				// Clip space Y up, like D3D and Metal.
				uint32 y = Value(OpCompositeExtract, context.float_type, { stored, 1 });
				uint32 flipped = Value(OpFNegate, context.float_type, { y });
				stored = Value(OpCompositeInsert, module[o[1]].type, { flipped, stored, 1 });
			}
			Emit(*out, OpStore, { Id(o[0]), stored });
			break;
		}
		case IROp::ACCESS:
		{
			Words indices;
			for (uint32 i = 1; i < o.count; ++i)
				indices.push_back(Id(o[i]));
			Result(ref, OpAccessChain, type, { Id(o[0]) }, indices);
			break;
		}
		case IROp::COPY:
		{
			// A load and store of the whole object rather than OpCopyMemory, like glslang.
			Type* pointee = ((PointerType*)module[o[1]].type)->pointee;
			uint32 loaded = Value(OpLoad, pointee, { Id(o[1]) });
			Emit(*out, OpStore, { Id(o[0]), loaded });
			break;
		}
		case IROp::CONSTRUCT:
		{
			Words parts;
			for (uint32 i = 0; i < o.count; ++i)
				parts.push_back(Id(o[i]));
			Result(ref, OpCompositeConstruct, type, {}, parts);
			break;
		}
		case IROp::EXTRACT: Result(ref, OpCompositeExtract, type, { Id(o[0]), Literal(o[1]) }); break;
		case IROp::EXTRACT_DYNAMIC: Result(ref, OpVectorExtractDynamic, type, { Id(o[0]), Id(o[1]) }); break;
		case IROp::SHUFFLE:
		{
			Words components;
			for (uint32 i = 2; i < o.count; ++i)
				components.push_back(Literal(o[i]));
			Result(ref, OpVectorShuffle, type, { Id(o[0]), Id(o[1]) }, components);
			break;
		}
		case IROp::ADD: Arithmetic(ref, OpIAdd, OpIAdd, OpFAdd, o); break;
		case IROp::SUB: Arithmetic(ref, OpISub, OpISub, OpFSub, o); break;
		case IROp::MUL: Arithmetic(ref, OpIMul, OpIMul, OpFMul, o); break;
		case IROp::DIV: Arithmetic(ref, OpSDiv, OpUDiv, OpFDiv, o); break;
		case IROp::REM:
			if (type->type_kind != TypeKind::MATRIX && GetScalarKind(type) == PrimitiveKind::SIGNED)
			{
				// a - b * (a / b): truncated like C and HLSL. OpSRem is undefined for negative operands in Vulkan without
				// VK_KHR_maintenance8, and gives wrong results on some drivers.
				uint32 a = Id(o[0]);
				uint32 b = Id(o[1]);
				uint32 quotient = Value(OpSDiv, type, { a, b });
				uint32 product = Value(OpIMul, type, { b, quotient });
				Result(ref, OpISub, type, { a, product });
				break;
			}
			Arithmetic(ref, OpUMod, OpUMod, OpFRem, o);
			break;
		case IROp::NEG: Arithmetic(ref, OpSNegate, OpSNegate, OpFNegate, o); break;
		case IROp::AND:
		case IROp::OR:
		case IROp::XOR:
		case IROp::NOT:
		{
			bool logical = GetScalarKind(type) == PrimitiveKind::BOOL;
			SpvOp op = OpNot;
			switch (value.op)
			{
			case IROp::AND: op = logical ? OpLogicalAnd : OpBitwiseAnd; break;
			case IROp::OR: op = logical ? OpLogicalOr : OpBitwiseOr; break;
			case IROp::XOR: op = logical ? OpLogicalNotEqual : OpBitwiseXor; break;
			default: op = logical ? OpLogicalNot : OpNot; break;
			}
			Words operands;
			for (uint32 i = 0; i < o.count; ++i)
				operands.push_back(Id(o[i]));
			Result(ref, op, type, {}, operands);
			break;
		}
		case IROp::SHL: Result(ref, OpShiftLeftLogical, type, { Id(o[0]), Id(o[1]) }); break;
		case IROp::SHR:
			Result(ref, GetScalarKind(type) == PrimitiveKind::SIGNED ? OpShiftRightArithmetic : OpShiftRightLogical, type,
				{ Id(o[0]), Id(o[1]) });
			break;
		case IROp::EQ: Compare(ref, OpIEqual, OpIEqual, OpFOrdEqual, OpLogicalEqual, o); break;
		case IROp::NE: Compare(ref, OpINotEqual, OpINotEqual, OpFUnordNotEqual, OpLogicalNotEqual, o); break;
		case IROp::LT: Compare(ref, OpSLessThan, OpULessThan, OpFOrdLessThan, OpSLessThan, o); break;
		case IROp::LE: Compare(ref, OpSLessThanEqual, OpULessThanEqual, OpFOrdLessThanEqual, OpSLessThanEqual, o); break;
		case IROp::GT: Compare(ref, OpSGreaterThan, OpUGreaterThan, OpFOrdGreaterThan, OpSGreaterThan, o); break;
		case IROp::GE: Compare(ref, OpSGreaterThanEqual, OpUGreaterThanEqual, OpFOrdGreaterThanEqual, OpSGreaterThanEqual, o); break;
		case IROp::SELECT: Select(ref, o); break;
		case IROp::CONVERT: Convert(ref, o[0]); break;
		case IROp::BITCAST:
			if (module[o[0]].type == type)
				ids[ref] = Id(o[0]);
			else
				Result(ref, OpBitcast, type, { Id(o[0]) });
			break;
		case IROp::MATMUL:
		{
			Type* left = module[o[0]].type;
			Type* right = module[o[1]].type;
			SpvOp op = OpMatrixTimesMatrix;
			if (left->type_kind != TypeKind::MATRIX)
				op = OpVectorTimesMatrix;
			else if (right->type_kind != TypeKind::MATRIX)
				op = OpMatrixTimesVector;
			Result(ref, op, type, { Id(o[0]), Id(o[1]) });
			break;
		}
		case IROp::INTRINSIC:
		{
			PrimitiveKind kind = GetScalarKind(type);
			uint32 instruction = 0;
			switch ((IRIntrinsic)value.sub_op)
			{
			case IRIntrinsic::MIN:
				instruction = kind == PrimitiveKind::FLOAT ? GLSL_FMIN : kind == PrimitiveKind::SIGNED ? GLSL_SMIN : GLSL_UMIN;
				break;
			case IRIntrinsic::MAX:
				instruction = kind == PrimitiveKind::FLOAT ? GLSL_FMAX : kind == PrimitiveKind::SIGNED ? GLSL_SMAX : GLSL_UMAX;
				break;
			case IRIntrinsic::DOT: break; // a core instruction
			case IRIntrinsic::LENGTH: instruction = GLSL_LENGTH; break;
			case IRIntrinsic::NORMALIZE: instruction = GLSL_NORMALIZE; break;
			}
			if ((IRIntrinsic)value.sub_op == IRIntrinsic::DOT)
			{
				Result(ref, OpDot, type, { Id(o[0]), Id(o[1]) });
				break;
			}
			Words arguments;
			for (uint32 i = 0; i < o.count; ++i)
				arguments.push_back(Id(o[i]));
			Result(ref, OpExtInst, type, { GlslStd(), instruction }, arguments);
			break;
		}
		case IROp::CALL:
		{
			Words arguments;
			for (uint32 i = 1; i < o.count; ++i)
				arguments.push_back(Id(o[i]));
			uint32 id = Value(OpFunctionCall, type ? type : context.void_type, { Id(o[0]) }, arguments);
			if (type)
				ids[ref] = id;
			break;
		}
		case IROp::SELECTION_MERGE: Emit(*out, OpSelectionMerge, { Id(o[0]), 0 }); break;
		case IROp::LOOP_MERGE: Emit(*out, OpLoopMerge, { Id(o[0]), Id(o[1]), 0 }); break;
		case IROp::BRANCH: Emit(*out, OpBranch, { Id(o[0]) }); break;
		case IROp::BRANCH_IF: Emit(*out, OpBranchConditional, { Id(o[0]), Id(o[1]), Id(o[2]) }); break;
		case IROp::RETURN:
			if (o.count)
				Emit(*out, OpReturnValue, { Id(o[0]) });
			else
				Emit(*out, OpReturn, {});
			break;
		case IROp::DISCARD: Emit(*out, OpKill, {}); break;
		case IROp::UNREACHABLE: Emit(*out, OpUnreachable, {}); break;
		default: Panic("SPIR-V: can't emit %s", GetIROpInfo(value.op).name);
		}
	}

	// Marks the blocks reachable from the entry block, and the merge and continue targets their headers name.
	void FindBlocks(IRRef function, std::vector<IRRef>& named)
	{
		std::vector<IRRef> stack = { module[function].function.first_block };
		reachable[stack[0]] = 1;
		while (!stack.empty())
		{
			IRRef block = stack.back();
			stack.pop_back();
			for (IRRef instruction = module[block].block.first; instruction; instruction = module[instruction].instruction.next)
			{
				IROp op = module[instruction].op;
				Slice<IRRef> o = GetIROperands(module, instruction);
				if (op == IROp::SELECTION_MERGE || op == IROp::LOOP_MERGE)
				{
					named.push_back(o[0]);
					if (op == IROp::LOOP_MERGE)
					{
						named.push_back(o[1]);
						continue_owner[o[1]] = block;
					}
					continue;
				}
				if (op != IROp::BRANCH && op != IROp::BRANCH_IF)
					continue;
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
	}

	void Function(IRRef function)
	{
		IRValue& value = module[function];
		IRFunctionInfo* info = value.function.info;
		FunctionType* type = (FunctionType*)value.type;
		out = &functions;

		Emit(functions, OpFunction, { TypeId(type->return_type), Id(function), 0, TypeId(type) });
		for (IRRef parameter = info->first_parameter; parameter; parameter = module[parameter].parameter.next)
		{
			ids[parameter] = NewId();
			Emit(functions, OpFunctionParameter, { TypeId(module[parameter].type), ids[parameter] });
		}

		std::vector<IRRef> named;
		FindBlocks(function, named);
		std::vector<uint8> emitted(module.count, 0); // reachable or named
		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
			emitted[block] = reachable[block];
		for (IRRef block : named)
			emitted[block] = 1;
		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
		{
			if (emitted[block])
				ids[block] = NewId();
		}

		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
		{
			if (!emitted[block])
				continue;
			Emit(functions, OpLabel, { ids[block] });
			if (block == value.function.first_block)
			{
				// Variables come first in the entry block. Without an initializer they start as zero.
				for (IRRef local = info->first_local; local; local = module[local].local.next)
				{
					PointerType* pointer = (PointerType*)module[local].type;
					Constant* initializer = module[local].local.initializer;
					uint32 initial = initializer ? ConstantId(initializer->type, initializer->bytes.data) : Null(pointer->pointee);
					ids[local] = NewId();
					Emit(functions, OpVariable, { TypeId(pointer), ids[local], STORAGE_FUNCTION, initial });
				}
			}
			if (!reachable[block])
			{
				// A merge or continue target nothing reaches. A continue target still has to branch back to its header.
				if (continue_owner[block])
					Emit(functions, OpBranch, { ids[continue_owner[block]] });
				else
					Emit(functions, OpUnreachable, {});
				continue;
			}
			for (IRRef instruction = module[block].block.first; instruction; instruction = module[instruction].instruction.next)
				Instruction(instruction);
		}
		Emit(functions, OpFunctionEnd, {});

		for (IRRef block = value.function.first_block; block; block = module[block].block.next)
		{
			reachable[block] = 0;
			continue_owner[block] = 0;
		}
	}

	void Global(IRRef global, EntryPoint* entry_point, Words& interface_ids)
	{
		IRValue& value = module[global];
		PointerType* pointer = (PointerType*)value.type;
		uint32 pointer_type = TypeId(pointer);
		uint32 id = NewId();
		ids[global] = id;
		if (pointer->space != AddressSpace::INPUT && pointer->space != AddressSpace::OUTPUT)
		{
			Constant* initializer = value.global.initializer;
			uint32 initial = initializer ? ConstantId(initializer->type, initializer->bytes.data) : Null(pointer->pointee);
			Emit(declarations, OpVariable, { pointer_type, id, STORAGE_PRIVATE, initial });
			return;
		}

		Emit(declarations, OpVariable, { pointer_type, id, GetStorageClass(pointer->space) });
		interface_ids.push_back(id);
		ShaderIO* io = value.global.io;
		if (io->io_kind == IOKind::LOCATION)
		{
			Emit(annotations, OpDecorate, { id, DECORATION_LOCATION, io->location });
			// Integer inputs of fragment shaders can't be interpolated.
			bool integer = GetScalarKind(io->type) != PrimitiveKind::FLOAT;
			if (entry_point->stage == ShaderStage::FRAGMENT && io->direction == IODirection::INPUT && integer)
				Emit(annotations, OpDecorate, { id, DECORATION_FLAT });
			return;
		}
		uint32 built_in = 0;
		switch (io->semantic)
		{
		case Semantic::VERTEX_INDEX: built_in = BUILT_IN_VERTEX_INDEX; break;
		case Semantic::POSITION: built_in = io->direction == IODirection::OUTPUT ? BUILT_IN_POSITION : BUILT_IN_FRAG_COORD; break;
		}
		Emit(annotations, OpDecorate, { id, DECORATION_BUILT_IN, built_in });
	}

	Slice<uint32> Module(IRRef wrapper, Arena* arena)
	{
		ids.assign(module.count, 0);
		reachable.assign(module.count, 0);
		continue_owner.assign(module.count, 0);
		EntryPoint* entry_point = module[wrapper].function.info->entry_point;
		assert(entry_point);

		IRReachable used;
		FindReachable(module, wrapper, used);
		for (IRRef function : used.functions)
			ids[function] = NewId();
		Words interface_ids;
		for (IRRef global : used.globals)
			Global(global, entry_point, interface_ids);

		// The wrapper first, like glslang's main, then its callers before their callees.
		for (size_t i = used.functions.size(); i-- > 0;)
			Function(used.functions[i]);

		uint32 model = entry_point->stage == ShaderStage::VERTEX ? EXECUTION_MODEL_VERTEX : EXECUTION_MODEL_FRAGMENT;
		Words name_and_interface = String("main");
		name_and_interface.insert(name_and_interface.end(), interface_ids.begin(), interface_ids.end());
		Emit(entry_points, OpEntryPoint, { model, ids[wrapper] }, name_and_interface);
		if (entry_point->stage == ShaderStage::FRAGMENT)
			Emit(execution_modes, OpExecutionMode, { ids[wrapper], EXECUTION_MODE_ORIGIN_UPPER_LEFT });

		Words words = { SPIRV_MAGIC, SPIRV_VERSION_1_0, 0, next_id, 0 };
		Emit(words, OpCapability, { CAPABILITY_SHADER });
		words.insert(words.end(), imports.begin(), imports.end());
		Emit(words, OpMemoryModel, { ADDRESSING_LOGICAL, MEMORY_MODEL_GLSL450 });
		for (Words* section : { &entry_points, &execution_modes, &annotations, &declarations, &functions })
			words.insert(words.end(), section->begin(), section->end());

		if (too_large)
		{
			EmitError(context, "a constant or type is too large for SPIR-V: it needs an instruction of %zu words, the limit is %u",
				too_large, MAX_INSTRUCTION_WORDS);
			return {};
		}

		uint32* data = (uint32*)arena->Allocate(words.size() * sizeof(uint32), alignof(uint32));
		memcpy(data, words.data(), words.size() * sizeof(uint32));
		return Slice<uint32>(data, (uint32)words.size());
	}
};

}

Slice<uint32> EmitSPIRV(IRModule& module, IRRef wrapper, Arena* arena)
{
	Emitter emitter = { .module = module, .context = *module.context };
	return emitter.Module(wrapper, arena);
}

}
