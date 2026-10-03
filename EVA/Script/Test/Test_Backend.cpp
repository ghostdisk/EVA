#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script_IR.hpp>
#include <EVA/Script/Test/OutputValidation.hpp>

using namespace EVA;
using namespace EVA::Script;
using GPU::Backend;
using GPU::CompiledEntryPoint;

// Backend tests that need more than a source file: IR the front end can't produce and the targets' limits. Compiling
// source and checking the output is done by the golden tests, Golden/Shader. Every output is checked with the target's
// own tools when they're available (OutputValidation.hpp): SPIRV-Tools' validator, fxc and Metal.

static StringView Text(CompiledEntryPoint& entry_point)
{
	return StringView((const char*)entry_point.code.data, entry_point.code.count);
}

static Slice<uint32> Words(CompiledEntryPoint& entry_point)
{
	return Slice<uint32>((uint32*)entry_point.code.data, entry_point.code.count / 4);
}

// The output's problem according to the target's tools, empty if there's none.
static ZTStringView Check(Test::Context& test, Backend target, CompiledEntryPoint& entry_point)
{
	if (target == Backend::VULKAN)
		return Validation::ValidateSPIRV(Words(entry_point), test.arena);
	if (target == Backend::METAL)
		return Validation::CompileMSL(Text(entry_point), test.arena);
	return Validation::CompileHLSL(Text(entry_point), entry_point.stage, test.arena);
}

// Compiles source for target. Every entry point has to compile and pass the target's tools. Returns them, or none with
// a failure reported.
static Slice<CompiledEntryPoint> Compile(Test::Context& test, const char* file, int line, const char* source, Backend target)
{
	CompileShaderResult result = CompileShader({ .arena = test.arena, .source = source, .backend = target });
	const char* target_name = target == Backend::VULKAN ? "SPIR-V" : target == Backend::METAL ? "MSL" : "HLSL";
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
		ZTStringView output = target == Backend::VULKAN ? Validation::DisassembleSPIRV(Words(entry_point), test.arena)
													  : InternString(test.arena, Text(entry_point));
		Test::ReportFailure(test, file, line, "\"%s\"\n    %s output for entry point %u is invalid: %s\n%s", source, target_name,
			i, problem.CString(), output.CString());
		return {};
	}
	return result.entry_points;
}

// IR the front end can't produce yet: a fragment entry point calling @f(%0: uint, %1: float4, %2: int): float4, whose
// body the test builds. Every output has to pass the targets' tools.
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

// A backend reported exactly the expected error, nullptr for none, and gave output only without one.
static bool CheckBackendError(Test::Context& test, const char* file, int line, const char* target, bool output,
	const std::vector<ScriptError*>& errors, const char* expected)
{
	ZTStringView got = errors.empty() ? ZTStringView() : errors[0]->message;
	if (got == StringView(expected) && errors.size() <= 1 && output == !expected)
		return true;
	Test::ReportFailure(test, file, line, "%s: got %zu errors, the first \"%s\", and %s output; expected the error \"%s\"",
		target, errors.size(), got.CString(), output ? "some" : "no", expected ? expected : "");
	return false;
}

static void CheckFixture(Test::Context& test, const char* file, int line, Fixture& f, const char* spirv_error = nullptr,
	const char* hlsl_error = nullptr, const char* msl_error = nullptr)
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
	std::vector<ScriptError*> errors;
	Slice<uint32> words = EmitSPIRV(f.module, f.wrapper, test.arena, errors);
	if (CheckBackendError(test, file, line, "SPIR-V", words.count != 0, errors, spirv_error) && !spirv_error)
	{
		entry_point.code = Slice<uint8>((uint8*)words.data, words.count * 4);
		ZTStringView problem = Check(test, Backend::VULKAN, entry_point);
		if (problem.length)
			Test::ReportFailure(test, file, line, "invalid SPIR-V: %s\n%s", problem.CString(),
				Validation::DisassembleSPIRV(words, test.arena).CString());
	}
	errors.clear();
	ZTStringView hlsl = EmitHLSL(f.module, f.wrapper, test.arena, errors);
	if (CheckBackendError(test, file, line, "HLSL", hlsl.length != 0, errors, hlsl_error) && !hlsl_error)
	{
		entry_point.code = Slice<uint8>(hlsl.data, (uint32)hlsl.length);
		ZTStringView problem = Check(test, Backend::D3D11, entry_point);
		if (problem.length)
			Test::ReportFailure(test, file, line, "invalid HLSL: %s\n%s", problem.CString(), hlsl.CString());
	}
	errors.clear();
	ZTStringView msl = EmitMSL(f.module, f.wrapper, test.arena, errors);
	if (CheckBackendError(test, file, line, "MSL", msl.length != 0, errors, msl_error) && !msl_error)
	{
		entry_point.code = Slice<uint8>(msl.data, (uint32)msl.length);
		ZTStringView problem = Check(test, Backend::METAL, entry_point);
		if (problem.length)
			Test::ReportFailure(test, file, line, "invalid MSL: %s\n%s", problem.CString(), msl.CString());
	}
}

#define CHECK_FIXTURE(f) CheckFixture(test, __FILE__, __LINE__, f)
#define CHECK_FIXTURE_ERRORS(f, spirv_error, hlsl_error) CheckFixture(test, __FILE__, __LINE__, f, spirv_error, hlsl_error)

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

TEST(Backend, ArrayTooLargeForHLSL)
{
	// fxc allows 65536 elements in an array, all its dimensions together. SPIR-V and MSL have no such limit for a variable.
	const char* source = "@entry(fragment) function PS(@location(0) i: uint): @location(0) float4 { v: [2][40000]float4; return v[i][1]; }";
	CompileShaderResult hlsl = CompileShader({ .arena = test.arena, .source = source, .backend = Backend::D3D11 });
	REQUIRE_EQ(hlsl.errors.count, 1u);
	CHECK_EQ(hlsl.errors[0]->message, "an array of 80000 elements is too large for HLSL, the limit is 65536");
	CHECK_EQ(hlsl.entry_points.count, 0u);
	Compile(test, __FILE__, __LINE__, source, Backend::VULKAN);
	Compile(test, __FILE__, __LINE__, source, Backend::METAL);
}

TEST(Backend, ConstantTooLargeForSPIRV)
{
	// One OpConstantComposite can't hold more than 65532 elements.
	Fixture f;
	f.Init(test.arena);
	Context& c = f.context;
	ArrayType* table_type = GetArrayType(c, c.float_type, 70000);
	Constant* table = test.arena->New<Constant>();
	table->type = table_type;
	uint8* bytes = (uint8*)test.arena->Allocate(table_type->size, 4);
	for (uint32 i = 0; i < 70000; ++i)
	{
		float value = (float)i;
		memcpy(bytes + (size_t)i * 4, &value, 4);
	}
	table->bytes = Slice<uint8>(bytes, table_type->size);
	IRRef global = AddIRGlobal(f.module, GetAtom("table"), AddressSpace::CONSTANT, table_type, table);
	IRRef element = f.Add(IROp::ACCESS, GetPointerType(c, AddressSpace::CONSTANT, c.float_type), { global, f.u });
	IRRef value = f.Add(IROp::LOAD, c.float_type, { element });
	f.Add(IROp::RETURN, nullptr, { f.Add(IROp::CONSTRUCT, f.float4, { value, value, value, value }) });
	CHECK_FIXTURE_ERRORS(f, "a constant or type is too large for SPIR-V: it needs an instruction of 70003 words, the limit is 65535",
		"an array of 70000 elements is too large for HLSL, the limit is 65536");
}
