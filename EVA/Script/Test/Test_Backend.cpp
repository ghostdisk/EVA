#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <EVA/Script/Test/OutputValidation.hpp>

using namespace EVA;
using namespace EVA::Script;

// Every output is checked with the target's own tools when they're available (OutputValidation.hpp): SPIRV-Tools'
// validator and fxc.

static StringView Text(CompiledEntryPoint& entry_point)
{
	return StringView((const char*)entry_point.code.data, entry_point.code.count);
}

static Slice<uint32> Words(CompiledEntryPoint& entry_point)
{
	return Slice<uint32>((uint32*)entry_point.code.data, entry_point.code.count / 4);
}

// The output's problem according to the target's tools, empty if there's none.
static ZTStringView Check(Test::Context& test, Target target, CompiledEntryPoint& entry_point)
{
	if (target == Target::SPIRV)
		return Validation::ValidateSPIRV(Words(entry_point), test.arena);
	return Validation::CompileHLSL(Text(entry_point), entry_point.stage, test.arena);
}

// Compiles source for target. Every entry point has to compile and pass the target's tools. Returns them, or none with
// a failure reported.
static Slice<CompiledEntryPoint> Compile(Test::Context& test, const char* file, int line, const char* source, Target target)
{
	CompileShaderResult result = CompileShader({ .arena = test.arena, .source = source, .target = target });
	const char* target_name = target == Target::SPIRV ? "SPIR-V" : "HLSL";
	if (result.errors.count)
	{
		Test::ReportFailure(test, file, line, "\"%s\"\n    failed to compile: %s", source, result.errors[0]->message.CString());
		return {};
	}
	for (uint32 i = 0; i < result.entry_points.count; ++i)
	{
		CompiledEntryPoint& entry_point = result.entry_points[i];
		ZTStringView problem = Check(test, target, entry_point);
		if (!problem.length)
			continue;
		ZTStringView output = target == Target::SPIRV ? Validation::DisassembleSPIRV(Words(entry_point), test.arena)
													  : InternString(test.arena, Text(entry_point));
		Test::ReportFailure(test, file, line, "\"%s\"\n    %s output for entry point %u is invalid: %s\n%s", source, target_name,
			i, problem.CString(), output.CString());
		return {};
	}
	return result.entry_points;
}

static void CheckValid(Test::Context& test, const char* file, int line, const char* source)
{
	Compile(test, file, line, source, Target::SPIRV);
	Compile(test, file, line, source, Target::HLSL);
}

// The output for each entry point, SPIR-V disassembled, separated by blank lines.
static void CheckOutput(Test::Context& test, const char* file, int line, const char* source, Target target, StringView expected)
{
	Slice<CompiledEntryPoint> entry_points = Compile(test, file, line, source, target);
	if (!entry_points.count)
		return;
	if (target == Target::SPIRV && !Validation::HaveSPIRVTools())
		return;
	StringBuilder builder(test.arena);
	for (uint32 i = 0; i < entry_points.count; ++i)
	{
		if (i)
			builder.Append("\n");
		builder.Append(target == Target::SPIRV ? Validation::DisassembleSPIRV(Words(entry_points[i]), test.arena)
											   : StringView(Text(entry_points[i])));
	}
	ZTStringView got = builder.ToString();
	if (got == expected)
		return;
	Test::ReportFailure(test, file, line, "\"%s\"\n    got\n%s\n    expected\n%.*s", source, got.CString(), (int)expected.length,
		(const char*)expected.data);
}

#define CHECK_BACKENDS(source) CheckValid(test, __FILE__, __LINE__, source)
#define CHECK_HLSL(source, expected) CheckOutput(test, __FILE__, __LINE__, source, Target::HLSL, expected)
#define CHECK_SPIRV(source, expected) CheckOutput(test, __FILE__, __LINE__, source, Target::SPIRV, expected)

static const char* TRIANGLE = R"(
const positions: [3]float2 = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };

@entry(vertex)
function VSMain(@semantic(vertex_index) vertex_id: uint): @semantic(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

@entry(fragment)
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)";

TEST(Backend, TriangleHLSL)
{
	CHECK_HLSL(TRIANGLE,
		"static const float2 g0[3] = { float2(0.0, 0.5), float2(0.5, (-0.5)), float2((-0.5), (-0.5)) };\n"
		"\n"
		"float4 f0(uint p0)\n"
		"{\n"
		"\tuint l0 = (uint)0;\n"
		"\tl0 = p0;\n"
		"\tuint v0 = l0;\n"
		"\tuint v1 = min(v0, 2u);\n"
		"\tfloat2 v2 = g0[v1];\n"
		"\tfloat4 v3 = float4(v2, 0.0, 1.0);\n"
		"\treturn v3;\n"
		"}\n"
		"\n"
		"void main(uint in0 : SV_VertexID, out float4 out0 : SV_Position)\n"
		"{\n"
		"\tuint v0 = in0;\n"
		"\tfloat4 v1 = f0(v0);\n"
		"\tout0 = v1;\n"
		"}\n"
		"\n"
		"float4 f0()\n"
		"{\n"
		"\tfloat4 v0 = float4(1.0, 1.0, 1.0, 1.0);\n"
		"\treturn v0;\n"
		"}\n"
		"\n"
		"void main(out float4 out0 : SV_Target0)\n"
		"{\n"
		"\tfloat4 v0 = f0();\n"
		"\tout0 = v0;\n"
		"}\n");
}

TEST(Backend, TriangleSPIRV)
{
	CHECK_SPIRV(TRIANGLE,
		"OpCapability Shader\n"
		"%35 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %2 \"main\" %18 %21\n"
		"OpDecorate %18 BuiltIn VertexIndex\n"
		"OpDecorate %21 BuiltIn Position\n"
		"%3 = OpTypeFloat 32\n"
		"%4 = OpTypeVector %3 2\n"
		"%5 = OpTypeInt 32 0\n"
		"%6 = OpConstant %5 3\n"
		"%7 = OpTypeArray %4 %6\n"
		"%8 = OpTypePointer Private %7\n"
		"%10 = OpConstant %3 0\n"
		"%11 = OpConstant %3 0.5\n"
		"%12 = OpConstantComposite %4 %10 %11\n"
		"%13 = OpConstant %3 -0.5\n"
		"%14 = OpConstantComposite %4 %11 %13\n"
		"%15 = OpConstantComposite %4 %13 %13\n"
		"%16 = OpConstantComposite %7 %12 %14 %15\n"
		"%9 = OpVariable %8 Private %16\n"
		"%17 = OpTypePointer Input %5\n"
		"%18 = OpVariable %17 Input\n"
		"%19 = OpTypeVector %3 4\n"
		"%20 = OpTypePointer Output %19\n"
		"%21 = OpVariable %20 Output\n"
		"%22 = OpTypeVoid\n"
		"%23 = OpTypeFunction %22\n"
		"%27 = OpTypeFunction %19 %5\n"
		"%30 = OpConstantNull %5\n"
		"%32 = OpTypePointer Function %5\n"
		"%34 = OpConstant %5 2\n"
		"%37 = OpTypePointer Private %4\n"
		"%40 = OpConstant %3 1\n"
		"%2 = OpFunction %22 None %23\n"
		"%24 = OpLabel\n"
		"%25 = OpLoad %5 %18\n"
		"%26 = OpFunctionCall %19 %1 %25\n"
		"OpStore %21 %26\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"%1 = OpFunction %19 None %27\n"
		"%28 = OpFunctionParameter %5\n"
		"%29 = OpLabel\n"
		"%31 = OpVariable %32 Function %30\n"
		"OpStore %31 %28\n"
		"%33 = OpLoad %5 %31\n"
		"%36 = OpExtInst %5 %35 UMin %33 %34\n"
		"%38 = OpAccessChain %37 %9 %36\n"
		"%39 = OpLoad %4 %38\n"
		"%41 = OpCompositeConstruct %19 %39 %10 %40\n"
		"OpReturnValue %41\n"
		"OpFunctionEnd\n"
		"\n"
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %2 \"main\" %6\n"
		"OpExecutionMode %2 OriginUpperLeft\n"
		"OpDecorate %6 Location 0\n"
		"%3 = OpTypeFloat 32\n"
		"%4 = OpTypeVector %3 4\n"
		"%5 = OpTypePointer Output %4\n"
		"%6 = OpVariable %5 Output\n"
		"%7 = OpTypeVoid\n"
		"%8 = OpTypeFunction %7\n"
		"%11 = OpTypeFunction %4\n"
		"%13 = OpConstant %3 1\n"
		"%2 = OpFunction %7 None %8\n"
		"%9 = OpLabel\n"
		"%10 = OpFunctionCall %4 %1\n"
		"OpStore %6 %10\n"
		"OpReturn\n"
		"OpFunctionEnd\n"
		"%1 = OpFunction %4 None %11\n"
		"%12 = OpLabel\n"
		"%14 = OpCompositeConstruct %4 %13 %13 %13 %13\n"
		"OpReturnValue %14\n"
		"OpFunctionEnd\n");
}

TEST(Backend, Interface)
{
	// Struct parameters and return values, locations of every kind, integers between stages.
	const char* source = R"(
struct Color { @location(1) value: float4; @location(2) id: uint; }
struct Varyings { @semantic(position) position: float4; @location(0) uv: float2; color: Color; }
@entry(vertex)
function VS(@semantic(vertex_index) id: uint, @location(3) offset: float2, @location(0) index: int): Varyings
{
	return { float4(offset, 0.0, 1.0), offset, { float4(1.0), id } };
}
@entry(fragment)
function PS(input: Varyings): @location(0) float4
{
	return input.color.value;
}
)";
	CHECK_BACKENDS(source);
	CHECK_HLSL(source,
		"struct S1\n"
		"{\n"
		"\tfloat4 m0;\n"
		"\tuint m1;\n"
		"};\n"
		"\n"
		"struct S0\n"
		"{\n"
		"\tfloat4 m0;\n"
		"\tfloat2 m1;\n"
		"\tS1 m2;\n"
		"};\n"
		"\n"
		"void f0(uint p0, float2 p1, int p2, inout S0 p3)\n"
		"{\n"
		"\tuint l0 = (uint)0;\n"
		"\tfloat2 l1 = (float2)0;\n"
		"\tint l2 = (int)0;\n"
		"\tl0 = p0;\n"
		"\tl1 = p1;\n"
		"\tl2 = p2;\n"
		"\tfloat2 v0 = l1;\n"
		"\tfloat4 v1 = float4(v0, 0.0, 1.0);\n"
		"\tp3.m0 = v1;\n"
		"\tfloat2 v2 = l1;\n"
		"\tp3.m1 = v2;\n"
		"\tfloat4 v3 = float4(1.0, 1.0, 1.0, 1.0);\n"
		"\tp3.m2.m0 = v3;\n"
		"\tuint v4 = l0;\n"
		"\tp3.m2.m1 = v4;\n"
		"}\n"
		"\n"
		"void main(int in0 : ATTRIB0, float2 in1 : ATTRIB3, uint in2 : SV_VertexID, out float2 out0 : TEXCOORD0, "
		"out float4 out1 : TEXCOORD1, nointerpolation out uint out2 : TEXCOORD2, out float4 out3 : SV_Position)\n"
		"{\n"
		"\tS0 l0 = (S0)0;\n"
		"\tuint v0 = in2;\n"
		"\tfloat2 v1 = in1;\n"
		"\tint v2 = in0;\n"
		"\tf0(v0, v1, v2, l0);\n"
		"\tfloat4 v3 = l0.m0;\n"
		"\tout3 = v3;\n"
		"\tfloat2 v4 = l0.m1;\n"
		"\tout0 = v4;\n"
		"\tfloat4 v5 = l0.m2.m0;\n"
		"\tout1 = v5;\n"
		"\tuint v6 = l0.m2.m1;\n"
		"\tout2 = v6;\n"
		"}\n"
		"\n"
		"struct S1\n"
		"{\n"
		"\tfloat4 m0;\n"
		"\tuint m1;\n"
		"};\n"
		"\n"
		"struct S0\n"
		"{\n"
		"\tfloat4 m0;\n"
		"\tfloat2 m1;\n"
		"\tS1 m2;\n"
		"};\n"
		"\n"
		"float4 f0(inout S0 p0)\n"
		"{\n"
		"\tfloat4 v0 = p0.m2.m0;\n"
		"\treturn v0;\n"
		"}\n"
		"\n"
		"void main(float2 in0 : TEXCOORD0, float4 in1 : TEXCOORD1, nointerpolation uint in2 : TEXCOORD2, "
		"float4 in3 : SV_Position, out float4 out0 : SV_Target0)\n"
		"{\n"
		"\tS0 l0 = (S0)0;\n"
		"\tfloat4 v0 = in3;\n"
		"\tl0.m0 = v0;\n"
		"\tfloat2 v1 = in0;\n"
		"\tl0.m1 = v1;\n"
		"\tfloat4 v2 = in1;\n"
		"\tl0.m2.m0 = v2;\n"
		"\tuint v3 = in2;\n"
		"\tl0.m2.m1 = v3;\n"
		"\tfloat4 v4 = f0(l0);\n"
		"\tout0 = v4;\n"
		"}\n");
}

TEST(Backend, Arithmetic)
{
	CHECK_BACKENDS(R"(
@entry(fragment)
function PS(@location(0) a: int, @location(1) b: uint, @location(2) c: float4): @location(0) float4
{
	a + 2 * a - a / 3 % a - -a;
	b / b + b % 7 - b * b;
	return -c * c / c % c - float4(1.5e-30) + float4(3.4e38);
}
)");
}

TEST(Backend, DivisionByFoldedZero)
{
	// Not constant expressions, so the front end allows them, but fxc folds them to a division by zero and rejects that
	// unless the divisor is guarded.
	CHECK_BACKENDS(R"(
@entry(fragment)
function PS(@location(0) a: int, @location(1) b: uint, @location(2) c: float): @location(0) float4
{
	a / 0;
	a % (a - a);
	b / (b * 0);
	b % 0;
	return float4(c / 0.0);
}
)");
}

TEST(Backend, FoldedOutOfBoundsIndex)
{
	// fxc folds the index and rejects an out of bounds constant, unless it's clamped.
	CHECK_BACKENDS(R"(
const table: [3]float4 = { float4(1.0), float4(2.0), float4(3.0) };
@entry(vertex)
function VS(@semantic(vertex_index) id: uint, @location(0) i: int): @semantic(position) float4
{
	local: [2]float4;
	return table[id * 0 + 5] + table[i - i - 1] + local[id + 7];
}
)");
}

TEST(Backend, Constants)
{
	// Aggregates of every kind, and floats that need care to print exactly.
	CHECK_BACKENDS(R"(
struct Inner { a: uint; b: [2]float2; }
struct Outer { inner: [2]Inner; c: int; d: float4; }
const outer: Outer = { { { 1, { float2(1.0, 2.0), float2(3.0, 4.0) } }, { 2, { float2(5.0), float2(6.0) } } }, -7, float4(1e-45, 1e-38, 3.4e38, -0.0) };
const nested: [2][3]int = { { 1, 2, 3 }, { -2147483647 - 1, 0, 2147483647 } };
struct Out { @location(0) x: int; @location(1) y: uint; }
@entry(fragment)
function PS(@location(0) i: uint): Out
{
	return { nested[i][i] + outer.c * nested[1][i], outer.inner[i].a };
}
@entry(vertex)
function VS(@location(0) i: uint): @semantic(position) float4
{
	return float4(outer.inner[i].b[i], 0.0, 1.0) + outer.d;
}
)");
}

TEST(Backend, Variables)
{
	// Zeroed locals, aggregate locals indexed dynamically, unreachable code, nested and unused functions.
	CHECK_BACKENDS(R"(
struct S { a: [4]float4; b: uint; }
function unused(): float { return 1.0; }
@entry(vertex)
function VS(@semantic(vertex_index) id: uint): @semantic(position) float4
{
	function nested(): int { return ~1; }
	v: S;
	w: [2][2]float4;
	return v.a[id] + w[id][1];
	return float4(2.0);
}
)");
}

// IR the front end can't produce yet: a fragment entry point calling @f(%0: uint, %1: float4, %2: int): float4, whose
// body the test builds. Both outputs have to pass the targets' tools.
namespace
{

struct Fixture
{
	Context context;
	IRModule module;
	ShaderIO io[4];
	EntryPoint entry_point = { .stage = ShaderStage::FRAGMENT };
	Type* float4 = nullptr;
	Type* bool4 = nullptr;
	IRRef function = 0;
	IRRef block = 0;
	IRRef u = 0;
	IRRef v = 0;
	IRRef i = 0;
	IRRef wrapper = 0;

	void Init(Arena* arena)
	{
		InitContext(context, arena, ContextKind::SHADER);
		InitIRModule(module, &context, arena);
		float4 = GetVectorType(context, context.float_type, 4);
		bool4 = GetVectorType(context, context.bool_type, 4);
		Type* parameters[] = { context.uint_type, float4, context.int_type };
		function = AddIRFunction(module, GetAtom("f"), GetFunctionType(context, float4, parameters), nullptr);
		u = GetIRParameter(module, function, 0);
		v = GetIRParameter(module, function, 1);
		i = GetIRParameter(module, function, 2);
		block = AddIRBlock(module, function);
	}

	IRRef Add(IROp op, Type* type, Slice<IRRef> operands, uint8 sub_op = 0)
	{
		return AddIRInstruction(module, block, op, type, operands, sub_op);
	}

	// Adds the wrapper: inputs at locations 0 to 2, the result at location 0.
	void Finish()
	{
		Type* types[] = { context.uint_type, float4, context.int_type };
		IRRef inputs[3];
		for (uint32 n = 0; n < 3; ++n)
		{
			io[n] = { .direction = IODirection::INPUT, .io_kind = IOKind::LOCATION, .location = n, .type = types[n] };
			inputs[n] = AddIRGlobal(module, GetAtom(aprintf(module.arena, "in%u", n)), AddressSpace::INPUT, types[n], nullptr);
			module[inputs[n]].global.io = &io[n];
		}
		io[3] = { .direction = IODirection::OUTPUT, .io_kind = IOKind::LOCATION, .location = 0, .type = float4 };
		IRRef output = AddIRGlobal(module, GetAtom("out0"), AddressSpace::OUTPUT, float4, nullptr);
		module[output].global.io = &io[3];
		entry_point.io = Slice<ShaderIO>(io, 4);

		wrapper = AddIRFunction(module, GetAtom("f.entry"), GetFunctionType(context, context.void_type, {}), nullptr);
		module[wrapper].function.info->entry_point = &entry_point;
		IRRef entry = AddIRBlock(module, wrapper);
		IRRef a = AddIRInstruction(module, entry, IROp::LOAD, context.uint_type, { inputs[0] });
		IRRef b = AddIRInstruction(module, entry, IROp::LOAD, float4, { inputs[1] });
		IRRef c = AddIRInstruction(module, entry, IROp::LOAD, context.int_type, { inputs[2] });
		IRRef result = AddIRInstruction(module, entry, IROp::CALL, float4, { function, a, b, c });
		AddIRInstruction(module, entry, IROp::STORE, nullptr, { output, result });
		AddIRInstruction(module, entry, IROp::RETURN, nullptr, {});
	}
};

}

static void CheckFixture(Test::Context& test, const char* file, int line, Fixture& f)
{
	f.Finish();
	ZTStringView error = ValidateIR(f.module, test.arena);
	if (error.length)
	{
		Test::ReportFailure(test, file, line, "the test's IR is invalid: %s\n%s", error.CString(),
			IRModuleToString(f.module, test.arena).CString());
		return;
	}
	ClampIndices(f.module);
	CompiledEntryPoint entry_point = { .stage = ShaderStage::FRAGMENT };
	Slice<uint32> words = EmitSPIRV(f.module, f.wrapper, test.arena);
	entry_point.code = Slice<uint8>((uint8*)words.data, words.count * 4);
	ZTStringView problem = Check(test, Target::SPIRV, entry_point);
	if (problem.length)
		Test::ReportFailure(test, file, line, "invalid SPIR-V: %s\n%s", problem.CString(),
			Validation::DisassembleSPIRV(words, test.arena).CString());
	ZTStringView hlsl = EmitHLSL(f.module, f.wrapper, test.arena);
	entry_point.code = Slice<uint8>(hlsl.data, (uint32)hlsl.length);
	problem = Check(test, Target::HLSL, entry_point);
	if (problem.length)
		Test::ReportFailure(test, file, line, "invalid HLSL: %s\n%s", problem.CString(), hlsl.CString());
}

#define CHECK_FIXTURE(f) CheckFixture(test, __FILE__, __LINE__, f)

TEST(Backend, Values)
{
	Fixture f;
	f.Init(test.arena);
	Context& c = f.context;
	Type* float2 = GetVectorType(c, c.float_type, 2);
	Type* int4 = GetVectorType(c, c.int_type, 4);
	Type* uint4 = GetVectorType(c, c.uint_type, 4);
	IRRef x = f.Add(IROp::EXTRACT, c.float_type, { f.v, GetIRUint(f.module, 0) });
	IRRef y = f.Add(IROp::EXTRACT_DYNAMIC, c.float_type, { f.v, f.u });
	IRRef xy = f.Add(IROp::CONSTRUCT, float2, { x, y });
	IRRef zw = f.Add(IROp::SHUFFLE, float2, { f.v, f.v, GetIRUint(f.module, 2), GetIRUint(f.module, 7) });
	IRRef sum = f.Add(IROp::ADD, float2, { xy, zw });
	IRRef doubled = f.Add(IROp::CONSTRUCT, f.float4, { sum, sum });
	IRRef less = f.Add(IROp::LT, f.bool4, { doubled, f.v });
	IRRef greater = f.Add(IROp::GE, f.bool4, { doubled, f.v });
	IRRef either = f.Add(IROp::OR, f.bool4, { less, greater });
	IRRef both = f.Add(IROp::AND, f.bool4, { less, either });
	IRRef different = f.Add(IROp::XOR, f.bool4, { both, f.Add(IROp::NOT, f.bool4, { less }) });
	IRRef chosen = f.Add(IROp::SELECT, f.float4, { different, doubled, f.v });
	IRRef three = f.Add(IROp::EQ, c.bool_type, { f.u, GetIRUint(f.module, 3) });
	IRRef scalar_choice = f.Add(IROp::SELECT, f.float4, { three, chosen, f.v });
	IRRef small = f.Add(IROp::INTRINSIC, f.float4, { scalar_choice, f.v }, (uint8)IRIntrinsic::MIN);
	IRRef large = f.Add(IROp::INTRINSIC, c.int_type, { f.i, GetIRInt(f.module, -3) }, (uint8)IRIntrinsic::MAX);
	IRRef smallest = f.Add(IROp::INTRINSIC, c.uint_type, { f.u, GetIRUint(f.module, 9) }, (uint8)IRIntrinsic::MIN);

	// Conversions between every kind, and bitcasts.
	IRRef as_int = f.Add(IROp::CONVERT, int4, { small });
	IRRef as_uint = f.Add(IROp::CONVERT, uint4, { as_int });
	IRRef as_bool = f.Add(IROp::CONVERT, f.bool4, { as_uint });
	IRRef float_bool = f.Add(IROp::CONVERT, f.bool4, { small });
	IRRef back = f.Add(IROp::CONVERT, f.float4, { as_bool });
	IRRef from_bool = f.Add(IROp::CONVERT, int4, { float_bool });
	IRRef bits = f.Add(IROp::BITCAST, uint4, { from_bool });
	IRRef same = f.Add(IROp::CONVERT, uint4, { bits });
	IRRef shift = f.Add(IROp::CONSTRUCT, uint4, { smallest, smallest, smallest, smallest });
	IRRef shifted = f.Add(IROp::SHR, uint4, { same, shift });
	IRRef signed_shift = f.Add(IROp::SHL, c.int_type, { large, f.i });
	IRRef arithmetic = f.Add(IROp::SHR, c.int_type, { signed_shift, f.i });
	IRRef masked = f.Add(IROp::AND, c.int_type, { arithmetic, f.i });
	IRRef inverted = f.Add(IROp::NOT, c.int_type, { f.i });
	IRRef negated = f.Add(IROp::NEG, c.int_type, { f.i });
	IRRef bitwise = f.Add(IROp::XOR, c.int_type, { masked, f.Add(IROp::OR, c.int_type, { inverted, negated }) });
	IRRef remainder = f.Add(IROp::REM, c.int_type, { bitwise, f.i });
	IRRef as_float = f.Add(IROp::BITCAST, f.float4, { shifted });
	IRRef scale = f.Add(IROp::CONVERT, c.float_type, { remainder });
	IRRef total = f.Add(IROp::ADD, f.float4, { back, as_float });
	IRRef divisor = f.Add(IROp::CONSTRUCT, f.float4, { scale, scale, scale, scale });
	f.Add(IROp::RETURN, nullptr, { f.Add(IROp::DIV, f.float4, { total, divisor }) });
	CHECK_FIXTURE(f);
}

TEST(Backend, ControlFlow)
{
	// if (u == 0) { local = v; if (i < 0) { return v } else { return local } } else {}
	// if (i == 5) { discard }
	// local[i] = 2.0; return local
	// The inner if's merge is unreachable, the second has no else, and a block after the last return is unreachable.
	Fixture f;
	f.Init(test.arena);
	Context& c = f.context;
	IRRef local = AddIRLocal(f.module, f.function, f.float4, nullptr);
	IRRef then_block = AddIRBlock(f.module, f.function);
	IRRef inner_then = AddIRBlock(f.module, f.function);
	IRRef inner_else = AddIRBlock(f.module, f.function);
	IRRef inner_merge = AddIRBlock(f.module, f.function);
	IRRef else_block = AddIRBlock(f.module, f.function);
	IRRef merge = AddIRBlock(f.module, f.function);
	IRRef discard = AddIRBlock(f.module, f.function);
	IRRef last = AddIRBlock(f.module, f.function);
	IRRef after = AddIRBlock(f.module, f.function);
	IRRef zero = f.Add(IROp::EQ, c.bool_type, { f.u, GetIRUint(f.module, 0) });
	f.Add(IROp::SELECTION_MERGE, nullptr, { merge });
	f.Add(IROp::BRANCH_IF, nullptr, { zero, then_block, else_block });

	AddIRInstruction(f.module, then_block, IROp::STORE, nullptr, { local, f.v });
	IRRef negative = AddIRInstruction(f.module, then_block, IROp::LT, c.bool_type, { f.i, GetIRInt(f.module, 0) });
	AddIRInstruction(f.module, then_block, IROp::SELECTION_MERGE, nullptr, { inner_merge });
	AddIRInstruction(f.module, then_block, IROp::BRANCH_IF, nullptr, { negative, inner_then, inner_else });
	AddIRInstruction(f.module, inner_then, IROp::RETURN, nullptr, { f.v });
	IRRef loaded = AddIRInstruction(f.module, inner_else, IROp::LOAD, f.float4, { local });
	AddIRInstruction(f.module, inner_else, IROp::RETURN, nullptr, { loaded });
	AddIRInstruction(f.module, inner_merge, IROp::UNREACHABLE, nullptr, {});
	AddIRInstruction(f.module, else_block, IROp::BRANCH, nullptr, { merge });

	IRRef five = AddIRInstruction(f.module, merge, IROp::EQ, c.bool_type, { f.i, GetIRInt(f.module, 5) });
	AddIRInstruction(f.module, merge, IROp::SELECTION_MERGE, nullptr, { last });
	AddIRInstruction(f.module, merge, IROp::BRANCH_IF, nullptr, { five, discard, last });
	AddIRInstruction(f.module, discard, IROp::DISCARD, nullptr, {});

	IRRef element = AddIRInstruction(f.module, last, IROp::ACCESS, GetPointerType(c, AddressSpace::FUNCTION, c.float_type),
		{ local, f.i });
	AddIRInstruction(f.module, last, IROp::STORE, nullptr, { element, GetIRFloat(f.module, 2.0f) });
	IRRef result = AddIRInstruction(f.module, last, IROp::LOAD, f.float4, { local });
	AddIRInstruction(f.module, last, IROp::RETURN, nullptr, { result });
	AddIRInstruction(f.module, after, IROp::UNREACHABLE, nullptr, {});
	CHECK_FIXTURE(f);
}
