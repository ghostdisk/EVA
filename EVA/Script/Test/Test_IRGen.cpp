#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script_IR.hpp>

using namespace EVA;
using namespace EVA::Script;

// Compiles source through the front end and IR gen. The IR has to validate. Returns its dump, or nullptr with a failure
// reported.
static const char* Generate(Test::Context& test, const char* file, int line, ContextKind kind, const char* source)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Context* context = test.arena->New<Context>();
	InitContext(*context, test.arena, kind);
	Resolver resolver = { .context = context, .arena = test.arena, .error_arena = test.arena };
	Typer typer = { .context = context, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	if (!Parse(parser, &module) || !Resolve(resolver, module) || !TypeCheck(typer, module))
	{
		Test::ReportFailure(test, file, line, "\"%s\"\n    failed in the front end", source);
		return nullptr;
	}
	ShaderInterface shader_interface;
	if (kind == ContextKind::SHADER)
	{
		ShaderInterfaceBuilder builder = { .arena = test.arena, .error_arena = test.arena };
		if (!BuildShaderInterface(builder, module, &shader_interface))
		{
			Test::ReportFailure(test, file, line, "\"%s\"\n    failed in the interface pass: %s", source,
				builder.errors[0]->message.CString());
			return nullptr;
		}
	}

	IRModule* ir = test.arena->New<IRModule>();
	InitIRModule(*ir, context, test.arena);
	GenerateIR(*ir, module, kind == ContextKind::SHADER ? &shader_interface : nullptr);
	ZTStringView dump = IRModuleToString(*ir, test.arena);
	ZTStringView error = ValidateIR(*ir, test.arena);
	if (error.length)
	{
		Test::ReportFailure(test, file, line, "\"%s\"\n    generated invalid IR: %s\n%s", source, error.CString(), dump.CString());
		return nullptr;
	}
	return dump.CString();
}

static void CheckGenerate(Test::Context& test, const char* file, int line, ContextKind kind, const char* source, StringView expected)
{
	const char* dump = Generate(test, file, line, kind, source);
	if (!dump || StringView(dump) == expected)
		return;
	Test::ReportFailure(test, file, line, "\"%s\"\n    got\n%s\n    expected\n%.*s", source, dump, (int)expected.length,
		(const char*)expected.data);
}

#define CHECK_IR(source, expected) CheckGenerate(test, __FILE__, __LINE__, ContextKind::SCRIPT, source, expected)
#define CHECK_SHADER_IR(source, expected) CheckGenerate(test, __FILE__, __LINE__, ContextKind::SHADER, source, expected)

TEST(IRGen, Triangle)
{
	CHECK_SHADER_IR(R"(
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
)",
		"global @positions: *constant [3]float2 = {(0.0, 0.5), (0.5, -0.5), (-0.5, -0.5)}\n"
		"global @VSMain.in0: *input uint semantic(vertex_index)\n"
		"global @VSMain.out0: *output float4 semantic(position)\n"
		"global @PSMain.out0: *output float4 location(0)\n"
		"\n"
		"function @VSMain(%0: uint): float4\n"
		"\tlocal $0: *function uint\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\t%1: uint = load $0\n"
		"\t%2: *constant float2 = access @positions, %1\n"
		"\t%3: float2 = load %2\n"
		"\t%4: float4 = construct %3, float 0.0, float 1.0\n"
		"\treturn %4\n"
		"\n"
		"function @PSMain(): float4\n"
		"block0:\n"
		"\t%0: float4 = construct float 1.0, float 1.0, float 1.0, float 1.0\n"
		"\treturn %0\n"
		"\n"
		"function @VSMain.entry(): void [entry vertex]\n"
		"block0:\n"
		"\t%0: uint = load @VSMain.in0\n"
		"\t%1: float4 = call @VSMain, %0\n"
		"\tstore @VSMain.out0, %1\n"
		"\treturn\n"
		"\n"
		"function @PSMain.entry(): void [entry fragment]\n"
		"block0:\n"
		"\t%0: float4 = call @PSMain\n"
		"\tstore @PSMain.out0, %0\n"
		"\treturn\n");
}

TEST(IRGen, StructInterface)
{
	// Struct inputs are stored into a local by path, struct outputs loaded from the returned one.
	CHECK_SHADER_IR(R"(
struct Color { @location(1) value: float4; @location(2) id: uint; }
struct Varyings { @semantic(position) position: float4; @location(0) uv: float2; color: Color; }
@entry(vertex)
function VS(@semantic(vertex_index) id: uint, @location(3) offset: float2): Varyings
{
	return { float4(offset, 0.0, 1.0), offset, { float4(1.0), id } };
}
@entry(fragment)
function PS(input: Varyings): @location(0) float4
{
	return input.color.value;
}
)",
		"global @VS.in0: *input uint semantic(vertex_index)\n"
		"global @VS.in1: *input float2 location(3)\n"
		"global @VS.out0: *output float4 semantic(position)\n"
		"global @VS.out1: *output float2 location(0)\n"
		"global @VS.out2: *output float4 location(1)\n"
		"global @VS.out3: *output uint location(2)\n"
		"global @PS.in0: *input float4 semantic(position)\n"
		"global @PS.in1: *input float2 location(0)\n"
		"global @PS.in2: *input float4 location(1)\n"
		"global @PS.in3: *input uint location(2)\n"
		"global @PS.out0: *output float4 location(0)\n"
		"\n"
		"function @VS(%0: uint, %1: float2, %2: *function Varyings): void\n"
		"\tlocal $0: *function uint\n"
		"\tlocal $1: *function float2\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\tstore $1, %1\n"
		"\t%3: *function float4 = access %2, uint 0\n"
		"\t%4: float2 = load $1\n"
		"\t%5: float4 = construct %4, float 0.0, float 1.0\n"
		"\tstore %3, %5\n"
		"\t%6: *function float2 = access %2, uint 1\n"
		"\t%7: float2 = load $1\n"
		"\tstore %6, %7\n"
		"\t%8: *function Color = access %2, uint 2\n"
		"\t%9: *function float4 = access %8, uint 0\n"
		"\t%10: float4 = construct float 1.0, float 1.0, float 1.0, float 1.0\n"
		"\tstore %9, %10\n"
		"\t%11: *function uint = access %8, uint 1\n"
		"\t%12: uint = load $0\n"
		"\tstore %11, %12\n"
		"\treturn\n"
		"\n"
		"function @PS(%0: *function Varyings): float4\n"
		"block0:\n"
		"\t%1: *function Color = access %0, uint 2\n"
		"\t%2: *function float4 = access %1, uint 0\n"
		"\t%3: float4 = load %2\n"
		"\treturn %3\n"
		"\n"
		"function @VS.entry(): void [entry vertex]\n"
		"\tlocal $0: *function Varyings\n"
		"block0:\n"
		"\t%0: uint = load @VS.in0\n"
		"\t%1: float2 = load @VS.in1\n"
		"\tcall @VS, %0, %1, $0\n"
		"\t%2: *function float4 = access $0, uint 0\n"
		"\t%3: float4 = load %2\n"
		"\tstore @VS.out0, %3\n"
		"\t%4: *function float2 = access $0, uint 1\n"
		"\t%5: float2 = load %4\n"
		"\tstore @VS.out1, %5\n"
		"\t%6: *function float4 = access $0, uint 2, uint 0\n"
		"\t%7: float4 = load %6\n"
		"\tstore @VS.out2, %7\n"
		"\t%8: *function uint = access $0, uint 2, uint 1\n"
		"\t%9: uint = load %8\n"
		"\tstore @VS.out3, %9\n"
		"\treturn\n"
		"\n"
		"function @PS.entry(): void [entry fragment]\n"
		"\tlocal $0: *function Varyings\n"
		"block0:\n"
		"\t%0: float4 = load @PS.in0\n"
		"\t%1: *function float4 = access $0, uint 0\n"
		"\tstore %1, %0\n"
		"\t%2: float2 = load @PS.in1\n"
		"\t%3: *function float2 = access $0, uint 1\n"
		"\tstore %3, %2\n"
		"\t%4: float4 = load @PS.in2\n"
		"\t%5: *function float4 = access $0, uint 2, uint 0\n"
		"\tstore %5, %4\n"
		"\t%6: uint = load @PS.in3\n"
		"\t%7: *function uint = access $0, uint 2, uint 1\n"
		"\tstore %7, %6\n"
		"\t%8: float4 = call @PS, $0\n"
		"\tstore @PS.out0, %8\n"
		"\treturn\n");
}

TEST(IRGen, Places)
{
	// Member access and indexing are accesses on places, loaded only at the leaf. Struct parameters are pointers.
	CHECK_IR("struct S { a: [2]float4; b: int; } function f(i: uint, s: S): float4 { v: S; return v.a[i] + s.a[1]; }",
		"function @f(%0: uint, %1: *function S): float4\n"
		"\tlocal $0: *function uint\n"
		"\tlocal $1: *function S\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\t%2: *function [2]float4 = access $1, uint 0\n"
		"\t%3: uint = load $0\n"
		"\t%4: *function float4 = access %2, %3\n"
		"\t%5: float4 = load %4\n"
		"\t%6: *function [2]float4 = access %1, uint 0\n"
		"\t%7: *function float4 = access %6, uint 1\n"
		"\t%8: float4 = load %7\n"
		"\t%9: float4 = add %5, %8\n"
		"\treturn %9\n");
}

TEST(IRGen, ConstantAggregates)
{
	// Array and struct consts become constant globals, copied whole or indexed.
	CHECK_IR("const t: [2]float = { 1.0, 2.0 }; function f(): [2]float { return t; } function g(): float { return t[1]; }",
		"global @t: *constant [2]float = {1.0, 2.0}\n"
		"\n"
		"function @f(%0: *function [2]float): void\n"
		"block0:\n"
		"\tcopy %0, @t\n"
		"\treturn\n"
		"\n"
		"function @g(): float\n"
		"block0:\n"
		"\t%0: *constant float = access @t, uint 1\n"
		"\t%1: float = load %0\n"
		"\treturn %1\n");
}

TEST(IRGen, Values)
{
	// Operands left to right, splats written out.
	CHECK_IR("function f(a: int, b: float2): float4 { return float4(-b, float2(1.0)) * float4(2.0) + float4(b, b); }",
		"function @f(%0: int, %1: float2): float4\n"
		"\tlocal $0: *function int\n"
		"\tlocal $1: *function float2\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\tstore $1, %1\n"
		"\t%2: float2 = load $1\n"
		"\t%3: float2 = neg %2\n"
		"\t%4: float2 = construct float 1.0, float 1.0\n"
		"\t%5: float4 = construct %3, %4\n"
		"\t%6: float4 = construct float 2.0, float 2.0, float 2.0, float 2.0\n"
		"\t%7: float4 = mul %5, %6\n"
		"\t%8: float2 = load $1\n"
		"\t%9: float2 = load $1\n"
		"\t%10: float4 = construct %8, %9\n"
		"\t%11: float4 = add %7, %10\n"
		"\treturn %11\n");
}

TEST(IRGen, ControlFlow)
{
	// Code after a return goes to an unreachable block, falling off the end returns zero, nested functions are hoisted.
	CHECK_IR("function f(): float { return 1.0; return 2.0; } function g(): float { } function h(): int { function n(): int { return ~1; } }",
		"function @f(): float\n"
		"block0:\n"
		"\treturn float 1.0\n"
		"block1:\n"
		"\treturn float 2.0\n"
		"\n"
		"function @g(): float\n"
		"block0:\n"
		"\treturn float 0.0\n"
		"\n"
		"function @h(): int\n"
		"block0:\n"
		"\treturn int 0\n"
		"\n"
		"function @n(): int\n"
		"block0:\n"
		"\t%0: int = not int 1\n"
		"\treturn %0\n");
}

TEST(IRGen, ExpressionStatements)
{
	CHECK_IR("function f(x: uint): uint { x; 1 + 2; return x % 3; }",
		"function @f(%0: uint): uint\n"
		"\tlocal $0: *function uint\n"
		"block0:\n"
		"\tstore $0, %0\n"
		"\t%1: uint = load $0\n"
		"\t%2: int = add int 1, int 2\n"
		"\t%3: uint = load $0\n"
		"\t%4: uint = rem %3, uint 3\n"
		"\treturn %4\n");
}
