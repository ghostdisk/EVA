#include <EVA/Script/Test/OutputValidation.hpp>
#include <EVA/Core/StringBuilder.hpp>
#include <math.h>
#include <string.h>

// SPIR-V disassembler for the tests and fuzzers, so their SPIR-V text doesn't depend on the Vulkan SDK. Prints exactly
// what SPIRV-Tools' disassembler prints without a header and with raw ids, for the instructions the backend emits and a
// few more. Anything else, an unknown opcode or enumerant, is printed as its raw numbers, so a gap shows up as a
// difference rather than being hidden.

namespace EVA::Script::Validation
{

namespace
{

// How each operand is printed, one character per operand:
//   T result type, R result id, i id, n literal number, s literal string, c literal typed by the result type
//   C capability, A addressing model, M memory model, E execution model, X execution mode, S storage class,
//   D decoration, B built-in, F function control, L selection control, P loop control, e extended instruction
//   p pairs of a literal and an id, to the end
// A '*' repeats the operand before it to the end. Operands left over are printed as literal numbers.
struct OpInfo
{
	uint16 opcode;
	const char* name;
	const char* operands;
};

const OpInfo ops[] = {
	{ 0, "OpNop", "" },
	{ 1, "OpUndef", "TR" },
	{ 5, "OpName", "is" },
	{ 6, "OpMemberName", "ins" },
	{ 10, "OpExtension", "s" },
	{ 11, "OpExtInstImport", "Rs" },
	{ 12, "OpExtInst", "TRiei*" },
	{ 14, "OpMemoryModel", "AM" },
	{ 15, "OpEntryPoint", "Eisi*" },
	{ 16, "OpExecutionMode", "iX" },
	{ 17, "OpCapability", "C" },
	{ 19, "OpTypeVoid", "R" },
	{ 20, "OpTypeBool", "R" },
	{ 21, "OpTypeInt", "Rnn" },
	{ 22, "OpTypeFloat", "Rn" },
	{ 23, "OpTypeVector", "Rin" },
	{ 24, "OpTypeMatrix", "Rin" },
	{ 28, "OpTypeArray", "Rii" },
	{ 29, "OpTypeRuntimeArray", "Ri" },
	{ 30, "OpTypeStruct", "Ri*" },
	{ 32, "OpTypePointer", "RSi" },
	{ 33, "OpTypeFunction", "Ri*" },
	{ 41, "OpConstantTrue", "TR" },
	{ 42, "OpConstantFalse", "TR" },
	{ 43, "OpConstant", "TRc" },
	{ 44, "OpConstantComposite", "TRi*" },
	{ 46, "OpConstantNull", "TR" },
	{ 54, "OpFunction", "TRFi" },
	{ 55, "OpFunctionParameter", "TR" },
	{ 56, "OpFunctionEnd", "" },
	{ 57, "OpFunctionCall", "TRi*" },
	{ 59, "OpVariable", "TRSi*" },
	{ 61, "OpLoad", "TRi" },
	{ 62, "OpStore", "ii" },
	{ 63, "OpCopyMemory", "ii" },
	{ 65, "OpAccessChain", "TRi*" },
	{ 66, "OpInBoundsAccessChain", "TRi*" },
	{ 71, "OpDecorate", "iD" },
	{ 72, "OpMemberDecorate", "inD" },
	{ 77, "OpVectorExtractDynamic", "TRii" },
	{ 78, "OpVectorInsertDynamic", "TRiii" },
	{ 79, "OpVectorShuffle", "TRii" },
	{ 80, "OpCompositeConstruct", "TRi*" },
	{ 81, "OpCompositeExtract", "TRi" },
	{ 82, "OpCompositeInsert", "TRii" },
	{ 83, "OpCopyObject", "TRi" },
	{ 84, "OpTranspose", "TRi" },
	{ 109, "OpConvertFToU", "TRi" },
	{ 110, "OpConvertFToS", "TRi" },
	{ 111, "OpConvertSToF", "TRi" },
	{ 112, "OpConvertUToF", "TRi" },
	{ 113, "OpUConvert", "TRi" },
	{ 114, "OpSConvert", "TRi" },
	{ 115, "OpFConvert", "TRi" },
	{ 124, "OpBitcast", "TRi" },
	{ 126, "OpSNegate", "TRi" },
	{ 127, "OpFNegate", "TRi" },
	{ 128, "OpIAdd", "TRii" },
	{ 129, "OpFAdd", "TRii" },
	{ 130, "OpISub", "TRii" },
	{ 131, "OpFSub", "TRii" },
	{ 132, "OpIMul", "TRii" },
	{ 133, "OpFMul", "TRii" },
	{ 134, "OpUDiv", "TRii" },
	{ 135, "OpSDiv", "TRii" },
	{ 136, "OpFDiv", "TRii" },
	{ 137, "OpUMod", "TRii" },
	{ 138, "OpSRem", "TRii" },
	{ 139, "OpSMod", "TRii" },
	{ 140, "OpFRem", "TRii" },
	{ 141, "OpFMod", "TRii" },
	{ 142, "OpVectorTimesScalar", "TRii" },
	{ 143, "OpMatrixTimesScalar", "TRii" },
	{ 144, "OpVectorTimesMatrix", "TRii" },
	{ 145, "OpMatrixTimesVector", "TRii" },
	{ 146, "OpMatrixTimesMatrix", "TRii" },
	{ 147, "OpOuterProduct", "TRii" },
	{ 148, "OpDot", "TRii" },
	{ 154, "OpAny", "TRi" },
	{ 155, "OpAll", "TRi" },
	{ 156, "OpIsNan", "TRi" },
	{ 157, "OpIsInf", "TRi" },
	{ 164, "OpLogicalEqual", "TRii" },
	{ 165, "OpLogicalNotEqual", "TRii" },
	{ 166, "OpLogicalOr", "TRii" },
	{ 167, "OpLogicalAnd", "TRii" },
	{ 168, "OpLogicalNot", "TRi" },
	{ 169, "OpSelect", "TRiii" },
	{ 170, "OpIEqual", "TRii" },
	{ 171, "OpINotEqual", "TRii" },
	{ 172, "OpUGreaterThan", "TRii" },
	{ 173, "OpSGreaterThan", "TRii" },
	{ 174, "OpUGreaterThanEqual", "TRii" },
	{ 175, "OpSGreaterThanEqual", "TRii" },
	{ 176, "OpULessThan", "TRii" },
	{ 177, "OpSLessThan", "TRii" },
	{ 178, "OpULessThanEqual", "TRii" },
	{ 179, "OpSLessThanEqual", "TRii" },
	{ 180, "OpFOrdEqual", "TRii" },
	{ 181, "OpFUnordEqual", "TRii" },
	{ 182, "OpFOrdNotEqual", "TRii" },
	{ 183, "OpFUnordNotEqual", "TRii" },
	{ 184, "OpFOrdLessThan", "TRii" },
	{ 185, "OpFUnordLessThan", "TRii" },
	{ 186, "OpFOrdGreaterThan", "TRii" },
	{ 187, "OpFUnordGreaterThan", "TRii" },
	{ 188, "OpFOrdLessThanEqual", "TRii" },
	{ 189, "OpFUnordLessThanEqual", "TRii" },
	{ 190, "OpFOrdGreaterThanEqual", "TRii" },
	{ 191, "OpFUnordGreaterThanEqual", "TRii" },
	{ 194, "OpShiftRightLogical", "TRii" },
	{ 195, "OpShiftRightArithmetic", "TRii" },
	{ 196, "OpShiftLeftLogical", "TRii" },
	{ 197, "OpBitwiseOr", "TRii" },
	{ 198, "OpBitwiseXor", "TRii" },
	{ 199, "OpBitwiseAnd", "TRii" },
	{ 200, "OpNot", "TRi" },
	{ 204, "OpBitReverse", "TRi" },
	{ 205, "OpBitCount", "TRi" },
	{ 207, "OpDPdx", "TRi" },
	{ 208, "OpDPdy", "TRi" },
	{ 209, "OpFwidth", "TRi" },
	{ 245, "OpPhi", "TRi*" },
	{ 246, "OpLoopMerge", "iiP" },
	{ 247, "OpSelectionMerge", "iL" },
	{ 248, "OpLabel", "R" },
	{ 249, "OpBranch", "i" },
	{ 250, "OpBranchConditional", "iii" },
	{ 251, "OpSwitch", "iip" },
	{ 252, "OpKill", "" },
	{ 253, "OpReturn", "" },
	{ 254, "OpReturnValue", "i" },
	{ 255, "OpUnreachable", "" },
};

struct Enumerant
{
	uint32 value;
	const char* name;
};

const Enumerant capabilities[] = {
	{ 0, "Matrix" }, { 1, "Shader" }, { 2, "Geometry" }, { 3, "Tessellation" }, { 4, "Addresses" }, { 5, "Linkage" },
	{ 6, "Kernel" }, { 9, "Float16" }, { 10, "Float64" }, { 11, "Int64" }, { 22, "Int16" }, { 39, "Int8" },
};

const Enumerant addressing_models[] = { { 0, "Logical" }, { 1, "Physical32" }, { 2, "Physical64" } };

const Enumerant memory_models[] = { { 0, "Simple" }, { 1, "GLSL450" }, { 2, "OpenCL" }, { 3, "Vulkan" } };

const Enumerant execution_models[] = {
	{ 0, "Vertex" }, { 1, "TessellationControl" }, { 2, "TessellationEvaluation" }, { 3, "Geometry" }, { 4, "Fragment" },
	{ 5, "GLCompute" }, { 6, "Kernel" },
};

const Enumerant execution_modes[] = {
	{ 0, "Invocations" }, { 1, "SpacingEqual" }, { 2, "SpacingFractionalEven" }, { 3, "SpacingFractionalOdd" },
	{ 4, "VertexOrderCw" }, { 5, "VertexOrderCcw" }, { 6, "PixelCenterInteger" }, { 7, "OriginUpperLeft" },
	{ 8, "OriginLowerLeft" }, { 9, "EarlyFragmentTests" }, { 10, "PointMode" }, { 11, "Xfb" }, { 12, "DepthReplacing" },
	{ 14, "DepthGreater" }, { 15, "DepthLess" }, { 16, "DepthUnchanged" }, { 17, "LocalSize" },
};

const Enumerant storage_classes[] = {
	{ 0, "UniformConstant" }, { 1, "Input" }, { 2, "Uniform" }, { 3, "Output" }, { 4, "Workgroup" },
	{ 5, "CrossWorkgroup" }, { 6, "Private" }, { 7, "Function" }, { 8, "Generic" }, { 9, "PushConstant" },
	{ 10, "AtomicCounter" }, { 11, "Image" }, { 12, "StorageBuffer" },
};

const Enumerant decorations[] = {
	{ 0, "RelaxedPrecision" }, { 1, "SpecId" }, { 2, "Block" }, { 3, "BufferBlock" }, { 4, "RowMajor" }, { 5, "ColMajor" },
	{ 6, "ArrayStride" }, { 7, "MatrixStride" }, { 8, "GLSLShared" }, { 9, "GLSLPacked" }, { 10, "CPacked" },
	{ 11, "BuiltIn" }, { 13, "NoPerspective" }, { 14, "Flat" }, { 15, "Patch" }, { 16, "Centroid" }, { 17, "Sample" },
	{ 18, "Invariant" }, { 19, "Restrict" }, { 20, "Aliased" }, { 21, "Volatile" }, { 22, "Constant" },
	{ 23, "Coherent" }, { 24, "NonWritable" }, { 25, "NonReadable" }, { 26, "Uniform" }, { 30, "Location" },
	{ 31, "Component" }, { 32, "Index" }, { 33, "Binding" }, { 34, "DescriptorSet" }, { 35, "Offset" },
	{ 42, "NoContraction" }, { 43, "InputAttachmentIndex" },
};

const uint32 DECORATION_BUILT_IN = 11;

const Enumerant built_ins[] = {
	{ 0, "Position" }, { 1, "PointSize" }, { 3, "ClipDistance" }, { 4, "CullDistance" }, { 5, "VertexId" },
	{ 6, "InstanceId" }, { 7, "PrimitiveId" }, { 8, "InvocationId" }, { 9, "Layer" }, { 10, "ViewportIndex" },
	{ 15, "FragCoord" }, { 16, "PointCoord" }, { 17, "FrontFacing" }, { 18, "SampleId" }, { 19, "SamplePosition" },
	{ 20, "SampleMask" }, { 22, "FragDepth" }, { 23, "HelperInvocation" }, { 24, "NumWorkgroups" },
	{ 25, "WorkgroupSize" }, { 26, "WorkgroupId" }, { 27, "LocalInvocationId" }, { 28, "GlobalInvocationId" },
	{ 29, "LocalInvocationIndex" }, { 42, "VertexIndex" }, { 43, "InstanceIndex" },
};

// Masks: each bit's name, in bit order.
const Enumerant function_controls[] = { { 1, "Inline" }, { 2, "DontInline" }, { 4, "Pure" }, { 8, "Const" } };
const Enumerant selection_controls[] = { { 1, "Flatten" }, { 2, "DontFlatten" } };
const Enumerant loop_controls[] = { { 1, "Unroll" }, { 2, "DontUnroll" }, { 4, "DependencyInfinite" }, { 8, "DependencyLength" } };

// GLSL.std.450, numbered from 1.
const char* const glsl_instructions[] = {
	"Round", "RoundEven", "Trunc", "FAbs", "SAbs", "FSign", "SSign", "Floor", "Ceil", "Fract", "Radians", "Degrees", "Sin",
	"Cos", "Tan", "Asin", "Acos", "Atan", "Sinh", "Cosh", "Tanh", "Asinh", "Acosh", "Atanh", "Atan2", "Pow", "Exp", "Log",
	"Exp2", "Log2", "Sqrt", "InverseSqrt", "Determinant", "MatrixInverse", "Modf", "ModfStruct", "FMin", "UMin", "SMin",
	"FMax", "UMax", "SMax", "FClamp", "UClamp", "SClamp", "FMix", "IMix", "Step", "SmoothStep", "Fma", "Frexp",
	"FrexpStruct", "Ldexp", "PackSnorm4x8", "PackUnorm4x8", "PackSnorm2x16", "PackUnorm2x16", "PackHalf2x16",
	"PackDouble2x32", "UnpackSnorm2x16", "UnpackUnorm2x16", "UnpackHalf2x16", "UnpackSnorm4x8", "UnpackUnorm4x8",
	"UnpackDouble2x32", "Length", "Distance", "Cross", "Normalize", "FaceForward", "Reflect", "Refract", "FindILsb",
	"FindSMsb", "FindUMsb", "InterpolateAtCentroid", "InterpolateAtSample", "InterpolateAtOffset", "NMin", "NMax", "NClamp",
};

const OpInfo* FindOp(uint32 opcode)
{
	for (const OpInfo& op : ops)
	{
		if (op.opcode == opcode)
			return &op;
	}
	return nullptr;
}

template <size_t N>
const char* FindName(const Enumerant (&enumerants)[N], uint32 value)
{
	for (const Enumerant& enumerant : enumerants)
	{
		if (enumerant.value == value)
			return enumerant.name;
	}
	return nullptr;
}

// What OpConstant needs to know about its type.
struct ScalarType
{
	enum Kind : uint8
	{
		NONE,
		INT,
		FLOAT,
	} kind = NONE;
	bool is_signed = false;
	uint32 width = 0;
};

struct Disassembler
{
	StringBuilder& builder;
	Slice<uint32> words;
	uint32 bound = 0;
	ScalarType* scalar_types = nullptr; // by id
	bool* glsl_sets = nullptr;          // by id, the GLSL.std.450 imports

	void Enum(const char* name, uint32 value)
	{
		if (name)
			builder.Append(name);
		else
			builder.AppendFormat("%u", value);
	}

	template <size_t N>
	void Mask(const Enumerant (&bits)[N], uint32 value)
	{
		if (!value)
		{
			builder.Append("None");
			return;
		}
		bool first = true;
		for (const Enumerant& bit : bits)
		{
			if (!(value & bit.value))
				continue;
			if (!first)
				builder.Append("|");
			builder.Append(bit.name);
			first = false;
			value &= ~bit.value;
		}
		if (value)
			builder.AppendFormat(first ? "%u" : "|%u", value);
	}

	// Like SPIRV-Tools' FloatProxy: normal numbers and zeros in decimal with enough digits to round trip, the rest as
	// hex floats.
	void Float(uint32 bits)
	{
		float value;
		memcpy(&value, &bits, 4);
		int kind = fpclassify(value);
		if (kind == FP_ZERO || kind == FP_NORMAL)
		{
			builder.AppendFormat("%.9g", (double)value);
			return;
		}
		const char* sign = bits & 0x80000000u ? "-" : "";
		uint32 exponent = (bits >> 23) & 0xff;
		uint32 fraction = (bits & 0x7fffff) << 1; // in 6 nibbles
		int32 int_exponent = (int32)exponent - 127;
		if (exponent == 0)
		{
			// Denormal: normalize, dropping the leading 1, which becomes implicit.
			while (!(fraction & 0x800000))
			{
				fraction <<= 1;
				int_exponent--;
			}
			fraction = (fraction << 1) & 0xffffff;
		}
		uint32 nibbles = 6;
		while (nibbles && !(fraction & 0xf))
		{
			fraction >>= 4;
			nibbles--;
		}
		builder.AppendFormat("%s0x1", sign);
		if (nibbles)
			builder.AppendFormat(".%0*x", (int)nibbles, fraction);
		builder.AppendFormat("p%s%d", int_exponent >= 0 ? "+" : "", int_exponent);
	}

	void String(Slice<uint32> operands, uint32& i)
	{
		const char* text = (const char*)&operands[i];
		size_t limit = (size_t)(operands.count - i) * 4;
		size_t length = strnlen(text, limit);
		builder.Append("\"");
		for (size_t c = 0; c < length; ++c)
		{
			if (text[c] == '"' || text[c] == '\\')
				builder.Append("\\");
			builder.Append(StringView(text + c, 1));
		}
		builder.Append("\"");
		i += (uint32)(length / 4 + 1);
	}

	void Id(uint32 id)
	{
		builder.AppendFormat("%%%u", id);
	}

	void Instruction(uint32 opcode, Slice<uint32> operands)
	{
		const OpInfo* op = FindOp(opcode);
		const char* kinds = op ? op->operands : "";
		uint32 result_type = 0;
		uint32 i = 0;

		// The result id goes first.
		const char* r = strchr(kinds, 'R');
		uint32 result_index = r ? (uint32)(r - kinds) : 0;
		if (r && result_index < operands.count)
		{
			Id(operands[result_index]);
			builder.Append(" = ");
		}
		if (op)
			builder.Append(op->name);
		else
			builder.AppendFormat("Op%u", opcode);

		for (const char* kind = kinds; *kind && i < operands.count; ++kind)
		{
			char k = *kind == '*' ? kind[-1] : *kind;
			if (*kind == '*')
				kind--; // repeats until the operands run out
			uint32 word = operands[i];
			if (k == 'R')
			{
				i++;
				continue;
			}
			builder.Append(" ");
			switch (k)
			{
			case 'T':
				result_type = word;
				Id(word);
				break;
			case 'i':
				Id(word);
				break;
			case 's':
				String(operands, i);
				continue;
			case 'c':
				Literal(result_type, operands, i);
				continue;
			case 'C': Enum(FindName(capabilities, word), word); break;
			case 'A': Enum(FindName(addressing_models, word), word); break;
			case 'M': Enum(FindName(memory_models, word), word); break;
			case 'E': Enum(FindName(execution_models, word), word); break;
			case 'X': Enum(FindName(execution_modes, word), word); break;
			case 'S': Enum(FindName(storage_classes, word), word); break;
			case 'B': Enum(FindName(built_ins, word), word); break;
			case 'F': Mask(function_controls, word); break;
			case 'L': Mask(selection_controls, word); break;
			case 'P': Mask(loop_controls, word); break;
			case 'D':
				Enum(FindName(decorations, word), word);
				if (word == DECORATION_BUILT_IN && i + 1 < operands.count)
				{
					builder.Append(" ");
					i++;
					Enum(FindName(built_ins, operands[i]), operands[i]);
				}
				break;
			case 'e':
			{
				uint32 set = operands[i - 1];
				const char* name = nullptr;
				if (set < bound && glsl_sets[set] && word >= 1 && word <= sizeof(glsl_instructions) / sizeof(glsl_instructions[0]))
					name = glsl_instructions[word - 1];
				Enum(name, word);
				break;
			}
			case 'p':
				builder.AppendFormat("%u", word);
				if (i + 1 < operands.count)
				{
					builder.Append(" ");
					Id(operands[++i]);
				}
				kind--; // to the end
				break;
			default:
				builder.AppendFormat("%u", word);
				break;
			}
			i++;
		}
		for (; i < operands.count; ++i)
			builder.AppendFormat(" %u", operands[i]);
		builder.Append("\n");

		Record(opcode, operands);
	}

	// OpConstant's value, printed by its type.
	void Literal(uint32 type, Slice<uint32> operands, uint32& i)
	{
		ScalarType scalar = type < bound ? scalar_types[type] : ScalarType();
		if (scalar.width == 32 && i < operands.count)
		{
			uint32 word = operands[i++];
			if (scalar.kind == ScalarType::FLOAT)
				Float(word);
			else if (scalar.is_signed)
				builder.AppendFormat("%d", (int32)word);
			else
				builder.AppendFormat("%u", word);
			return;
		}
		for (bool first = true; i < operands.count; ++i, first = false)
			builder.AppendFormat(first ? "%u" : " %u", operands[i]);
	}

	// Remembers what later instructions need: scalar types for constants, GLSL.std.450 imports for extended
	// instructions.
	void Record(uint32 opcode, Slice<uint32> operands)
	{
		if (!operands.count || operands[0] >= bound)
			return;
		uint32 id = operands[0];
		if (opcode == 21 && operands.count == 3)
			scalar_types[id] = { .kind = ScalarType::INT, .is_signed = operands[2] != 0, .width = operands[1] };
		else if (opcode == 22 && operands.count >= 2)
			scalar_types[id] = { .kind = ScalarType::FLOAT, .width = operands[1] };
		else if (opcode == 11 && operands.count >= 2)
		{
			const char* name = (const char*)&operands[1];
			size_t limit = (size_t)(operands.count - 1) * 4;
			glsl_sets[id] = strnlen(name, limit) == 12 && memcmp(name, "GLSL.std.450", 12) == 0;
		}
	}
};

}

ZTStringView DisassembleSPIRV(Slice<uint32> words, Arena* arena)
{
	if (words.count < 5 || words[0] != 0x07230203)
		return aprintf(arena, "can't disassemble: not a SPIR-V module");
	uint32 bound = words[3];
	if (bound > (1u << 22))
		return aprintf(arena, "can't disassemble: id bound %u is too large", bound);
	StringBuilder builder(arena);
	Disassembler disassembler = { .builder = builder, .words = words, .bound = bound };
	disassembler.scalar_types = (ScalarType*)arena->Allocate(sizeof(ScalarType) * bound, alignof(ScalarType));
	memset((void*)disassembler.scalar_types, 0, sizeof(ScalarType) * bound);
	disassembler.glsl_sets = (bool*)arena->Allocate(bound);
	memset(disassembler.glsl_sets, 0, bound);

	for (uint32 offset = 5; offset < words.count;)
	{
		uint32 count = words[offset] >> 16;
		uint32 opcode = words[offset] & 0xffff;
		if (count == 0 || offset + count > words.count)
			return aprintf(arena, "can't disassemble: instruction at word %u has a bad length", offset);
		disassembler.Instruction(opcode, Slice<uint32>(words.data + offset + 1, count - 1));
		offset += count;
	}
	return builder.ToString();
}

}
