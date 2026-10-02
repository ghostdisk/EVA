#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>

using namespace EVA;
using namespace EVA::Script;

// Builds the shader interface of source, which must parse, resolve and type without errors. Returns false with a
// failure reported if it doesn't. The interface's errors go to out_errors joined with " | ", empty if there were none.
static bool BuildInterface(Test::Context& test, const char* file, int line, const char* source, ShaderInterface* out_interface,
	ZTStringView* out_errors)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	Context* context = test.arena->New<Context>(); // outlives this function along with the interface referencing its types
	InitContext(*context, test.arena, ContextKind::SHADER);
	Resolver resolver = { .context = context, .arena = test.arena, .error_arena = test.arena };
	Typer typer = { .context = context, .arena = test.arena, .error_arena = test.arena };
	if (!Parse(parser, &module) || !Resolve(resolver, module) || !TypeCheck(typer, module))
	{
		Test::ReportFailure(test, file, line, "\"%s\"\n    failed before the interface pass", source);
		return false;
	}

	ShaderInterfaceBuilder builder = { .arena = test.arena, .error_arena = test.arena };
	BuildShaderInterface(builder, module, out_interface);
	StringBuilder errors(test.arena);
	for (size_t i = 0; i < builder.errors.size(); ++i)
	{
		if (i)
			errors.Append(" | ");
		errors.Append(builder.errors[i]->message);
	}
	*out_errors = errors.ToString();
	return true;
}

// Source must build without errors to the expected interface, printed by ShaderInterfaceToString.
static void CheckInterface(Test::Context& test, const char* file, int line, const char* source, StringView expected)
{
	ShaderInterface shader_interface;
	ZTStringView errors;
	if (!BuildInterface(test, file, line, source, &shader_interface, &errors))
		return;
	if (errors.length)
	{
		Test::ReportFailure(test, file, line, "\"%s\"\n    failed with %s", source, errors.CString());
		return;
	}
	ZTStringView got = ShaderInterfaceToString(shader_interface, test.arena);
	if (got == expected)
		return;
	Test::ReportFailure(test, file, line, "\"%s\"\n    got\n%s    expected\n%.*s", source, got.CString(), (int)expected.length,
		(const char*)expected.data);
}

// Source must build with exactly these errors, joined with " | ".
static void CheckInterfaceErrors(Test::Context& test, const char* file, int line, const char* source, StringView expected)
{
	ShaderInterface shader_interface;
	ZTStringView errors;
	if (!BuildInterface(test, file, line, source, &shader_interface, &errors))
		return;
	if (errors == expected)
		return;
	Test::ReportFailure(test, file, line, "\"%s\"\n    got errors      %s\n    expected errors %.*s", source, errors.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_INTERFACE(source, expected) CheckInterface(test, __FILE__, __LINE__, source, expected)
#define CHECK_INTERFACE_ERRORS(source, expected) CheckInterfaceErrors(test, __FILE__, __LINE__, source, expected)

TEST(ShaderInterface, TriangleShader)
{
	CHECK_INTERFACE(R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

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
		"vertex VSMain\n"
		"  input semantic(vertex_index) uint [0]\n"
		"  output semantic(position) float4 []\n"
		"fragment PSMain\n"
		"  output location(0) float4 []\n");
}

TEST(ShaderInterface, EntryPoints)
{
	CHECK_INTERFACE("", "");
	CHECK_INTERFACE("function f() {}", "");
	// In source order, any number per stage.
	CHECK_INTERFACE("@entry(fragment) function b() {} @entry(fragment) function a() {} function c() {}",
		"fragment b\n"
		"fragment a\n");
	// A fragment shader doesn't need outputs.
	CHECK_INTERFACE("@entry(fragment) function f(): void {}", "fragment f\n");
	CHECK_INTERFACE("const SLOT: uint = 7; @entry(fragment) function f(@location(SLOT) a: float, @location(31) b: int) {}",
		"fragment f\n"
		"  input location(7) float [0]\n"
		"  input location(31) int [1]\n");
	CHECK_INTERFACE("@entry(fragment) function f(@semantic(position) p: float4): @location(0) float4 { return p; }",
		"fragment f\n"
		"  input semantic(position) float4 [0]\n"
		"  output location(0) float4 []\n");
}

TEST(ShaderInterface, Structs)
{
	// Flattened into their leaves, at any depth, with the path to each.
	CHECK_INTERFACE(R"(
struct Inner { @location(1) b: float; @semantic(position) p: float4; }
struct In { @location(0) a: float2; inner: Inner; }
struct Out { @location(0) color: float4; @location(3) id: uint; }
@entry(fragment) function f(i: In, @location(2) x: int): Out { return { float4(1.0), 1 }; }
)",
		"fragment f\n"
		"  input location(0) float2 [0, 0]\n"
		"  input location(1) float [0, 1, 0]\n"
		"  input semantic(position) float4 [0, 1, 1]\n"
		"  input location(2) int [1]\n"
		"  output location(0) float4 [0]\n"
		"  output location(3) uint [1]\n");
	CHECK_INTERFACE(R"(
struct Color { @location(0) value: float4; }
struct Out { color: Color; @semantic(position) position: float4; }
@entry(vertex) function v(): Out { return { { float4(1.0) }, float4(0.0) }; }
)",
		"vertex v\n"
		"  output location(0) float4 [0, 0]\n"
		"  output semantic(position) float4 [1]\n");
	// A struct can be shared by entry points and ordinary functions.
	CHECK_INTERFACE(R"(
struct V { @semantic(position) position: float4; @location(0) uv: float2; }
function g(v: V): V { return v; }
@entry(vertex) function vs(): V { return { float4(0.0), float2(0.0) }; }
@entry(fragment) function ps(v: V) {}
)",
		"vertex vs\n"
		"  output semantic(position) float4 [0]\n"
		"  output location(0) float2 [1]\n"
		"fragment ps\n"
		"  input semantic(position) float4 [0, 0]\n"
		"  input location(0) float2 [0, 1]\n");
}

TEST(ShaderInterface, Records)
{
	ShaderInterface shader_interface;
	ZTStringView errors;
	const char* source = "struct S { @location(4) b: float2; }\n"
						 "@entry(vertex) function v(@semantic(vertex_index) a: uint, s: S): @semantic(position) float4 { return float4(1.0); }";
	REQUIRE(BuildInterface(test, __FILE__, __LINE__, source, &shader_interface, &errors));
	CHECK_EQ(errors.length, (size_t)0);
	REQUIRE_EQ(shader_interface.entry_points.count, 1u);
	EntryPoint& entry_point = shader_interface.entry_points[0];
	CHECK_EQ(entry_point.stage, ShaderStage::VERTEX);
	CHECK_EQ(entry_point.function->node_type, NodeType::FUNCTION);
	CHECK_EQ(entry_point.function->name, GetAtom("v"));
	REQUIRE_EQ(entry_point.io.count, 3u);

	ShaderIO& a = entry_point.io[0];
	CHECK_EQ(a.direction, IODirection::INPUT);
	CHECK_EQ(a.io_kind, IOKind::SEMANTIC);
	CHECK_EQ(a.semantic, Semantic::VERTEX_INDEX);
	CHECK_EQ(a.declaration->node_type, NodeType::PARAMETER);
	CHECK_EQ(a.declaration->name, GetAtom("a"));
	CHECK(a.type == a.declaration->type);

	ShaderIO& b = entry_point.io[1];
	CHECK_EQ(b.direction, IODirection::INPUT);
	CHECK_EQ(b.io_kind, IOKind::LOCATION);
	CHECK_EQ(b.location, 4u);
	CHECK_EQ(b.declaration->node_type, NodeType::FIELD);
	CHECK_EQ(b.declaration->name, GetAtom("b"));
	CHECK(b.type == b.declaration->type);
	REQUIRE_EQ(b.path.count, 2u);
	CHECK_EQ(b.path[0], 1u);
	CHECK_EQ(b.path[1], 0u);

	ShaderIO& position = entry_point.io[2];
	CHECK_EQ(position.direction, IODirection::OUTPUT);
	CHECK_EQ(position.semantic, Semantic::POSITION);
	CHECK_EQ(position.declaration->usage, Usage::RETURN_TYPE);
	CHECK_EQ(position.path.count, 0u);
}

TEST(ShaderInterface, EntryPointErrors)
{
	CHECK_INTERFACE_ERRORS("@entry(vertex) @entry(fragment) function f() {}", "'f' can only have one 'entry'");
	CHECK_INTERFACE_ERRORS("@entry(fragment) @entry(fragment) function f() {}", "'f' can only have one 'entry'");
	CHECK_INTERFACE_ERRORS("function g() { @entry(fragment) function f() {} }", "entry point 'f' has to be declared at the top level");
	CHECK_INTERFACE_ERRORS("function g(@location(0) a: float) {}",
		"'g' isn't an entry point, so its parameters and return value can't have a semantic or location");
	CHECK_INTERFACE_ERRORS("function g(): @semantic(position) float4 { return float4(1.0); }",
		"'g' isn't an entry point, so its parameters and return value can't have a semantic or location");
	// Nested functions are checked too, inside entry points or not.
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f() { function g(@location(0) a: float) {} }",
		"'g' isn't an entry point, so its parameters and return value can't have a semantic or location");
	CHECK_INTERFACE_ERRORS("@entry(vertex) function v() {}", "vertex shader 'v' has to output @semantic(position)");
	CHECK_INTERFACE_ERRORS("@entry(vertex) function v(): @location(0) float4 { return float4(1.0); }",
		"vertex shader 'v' has to output @semantic(position)");
}

TEST(ShaderInterface, IOErrors)
{
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(a: float) {}", "'a' needs a semantic or location");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(): float4 { return float4(1.0); }", "the return value needs a semantic or location");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(0) @location(1) a: float) {}", "'a' can only have one semantic or location");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(0) @semantic(position) a: float4) {}",
		"'a' can only have one semantic or location");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(0) a: [2]float) {}", "'a' is [2]float, which can't be an input");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(): @location(0) void {}", "the return value is void, which can't be an output");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(32) a: float) {}", "location 32 is out of range, the limit is 31");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(4294967295) a: float) {}",
		"location 4294967295 is out of range, the limit is 31");
	// Each parameter reports its own errors.
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(a: float, b: float) {}",
		"'a' needs a semantic or location | 'b' needs a semantic or location");
}

TEST(ShaderInterface, SemanticErrors)
{
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@semantic(vertex_index) i: uint) {}",
		"'vertex_index' can't be an input of a fragment shader");
	CHECK_INTERFACE_ERRORS("@entry(vertex) function v(@semantic(position) p: float4): @semantic(position) float4 { return p; }",
		"'position' can't be an input of a vertex shader");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(): @semantic(position) float4 { return float4(1.0); }",
		"'position' can't be an output of a fragment shader");
	CHECK_INTERFACE_ERRORS("@entry(vertex) function v(@semantic(vertex_index) i: int): @semantic(position) float4 { return float4(1.0); }",
		"'vertex_index' must be uint, got int");
	CHECK_INTERFACE_ERRORS("@entry(vertex) function v(): @semantic(position) float3 { return float3(1.0); }",
		"'position' must be float4, got float3");
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@semantic(position) p: float) {}", "'position' must be float4, got float");
}

TEST(ShaderInterface, Duplicates)
{
	CHECK_INTERFACE_ERRORS("@entry(fragment) function f(@location(0) a: float, @location(0) b: float) {}",
		"location 0 is used twice in the inputs");
	CHECK_INTERFACE_ERRORS(R"(
struct Out { @semantic(position) a: float4; @semantic(position) b: float4; }
@entry(vertex) function v(): Out { return { float4(1.0), float4(1.0) }; }
)",
		"'position' is used twice in the outputs");
	// Inputs and outputs are separate, so are entry points.
	CHECK_INTERFACE_ERRORS(R"(
@entry(fragment) function f(@location(0) a: float4): @location(0) float4 { return a; }
@entry(fragment) function g(@location(0) a: float4): @location(0) float4 { return a; }
)",
		"");
}

TEST(ShaderInterface, StructErrors)
{
	CHECK_INTERFACE_ERRORS("struct S { @location(0) a: float; } @entry(fragment) function f(@location(1) s: S) {}",
		"'s' is a struct, only its fields can have a semantic or location");
	CHECK_INTERFACE_ERRORS("struct S { a: float; } @entry(fragment) function f(s: S) {}", "'a' needs a semantic or location");
	CHECK_INTERFACE_ERRORS("struct E {} @entry(fragment) function f(e: E) {}", "'e' is an empty struct, which can't be an input");
	CHECK_INTERFACE_ERRORS("struct E {} @entry(fragment) function f(): E { return {}; }",
		"the return value is an empty struct, which can't be an output");
	// The first error in a parameter stops its walk.
	CHECK_INTERFACE_ERRORS("struct S { a: float; b: float; } @entry(fragment) function f(s: S) {}", "'a' needs a semantic or location");
}

// Structs of two fields nested 24 deep have 2^24 leaves. The walk has to stop early instead of visiting them all.
static const char* DoublingStructs(Test::Context& test, const char* leaf)
{
	StringBuilder builder(test.arena);
	for (uint32 i = 0; i < 24; ++i)
		builder.AppendFormat("struct S%u { a: S%u; b: S%u; }\n", i, i + 1, i + 1);
	builder.AppendFormat("struct S24 { %s }\n@entry(fragment) function f(s: S0) {}", leaf);
	return builder.ToString().CString();
}

TEST(ShaderInterface, ExponentialStructs)
{
	CHECK_INTERFACE_ERRORS(DoublingStructs(test, "@location(0) x: float;"), "location 0 is used twice in the inputs");
	CHECK_INTERFACE_ERRORS(DoublingStructs(test, ""), "'a' is an empty struct, which can't be an input");
	CHECK_INTERFACE_ERRORS(DoublingStructs(test, "x: float;"), "'x' needs a semantic or location");
}
