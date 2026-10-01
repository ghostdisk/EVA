#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>

using namespace EVA;
using namespace EVA::Script;

// Parses and resolves source. Returns the module, or nullptr with a "parse error: <message>" in out_errors.
// Resolve errors go to out_errors joined with " | ", empty if there were none.
static Node* ParseAndResolve(Arena* arena, const char* source, ZTStringView* out_errors)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = arena, .error_arena = arena };
	Node* module = nullptr;
	if (!Parse(parser, &module))
	{
		*out_errors = aprintf(arena, "parse error: %s", parser.errors.back()->message.CString());
		return nullptr;
	}

	Context context;
	InitContext(context, arena); // in arena, so the types outlive this function along with the tree referencing them
	Resolver resolver = { .context = &context, .arena = arena, .error_arena = arena };
	Resolve(resolver, module);

	StringBuilder builder(arena);
	for (size_t i = 0; i < resolver.errors.size(); ++i)
	{
		if (i)
			builder.Append(" | ");
		builder.Append(resolver.errors[i]->message);
	}
	*out_errors = builder.ToString();
	return module;
}

// Source must resolve without errors to the expected declarations, serialized like the parser tests.
static void CheckResolve(Test::Context& test, const char* file, int line, const char* source, StringView expected)
{
	ZTStringView errors;
	Node* module = ParseAndResolve(test.arena, source, &errors);
	if (!module || errors.length)
	{
		Test::ReportFailure(test, file, line, "resolving \"%s\"\n    failed with %s", source, errors.CString());
		return;
	}

	StringBuilder builder(test.arena);
	for (Node* declaration = module->child; declaration; declaration = declaration->next)
	{
		if (declaration != module->child)
			builder.Append(" ");
		SerializeNode(builder, declaration);
	}
	if (builder.ToString() == expected)
		return;
	Test::ReportFailure(test, file, line, "resolving \"%s\"\n    got      %s\n    expected %.*s", source, builder.ToString().CString(),
		(int)expected.length, (const char*)expected.data);
}

// Source must resolve with exactly these errors, joined with " | ". "" for none.
static void CheckResolveErrors(Test::Context& test, const char* file, int line, const char* source, StringView expected)
{
	ZTStringView errors;
	ParseAndResolve(test.arena, source, &errors);
	if (errors == expected)
		return;
	Test::ReportFailure(test, file, line, "resolving \"%s\"\n    got errors      %s\n    expected errors %.*s", source, errors.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_RESOLVE(source, expected) CheckResolve(test, __FILE__, __LINE__, source, expected)
#define CHECK_RESOLVE_ERRORS(source, expected) CheckResolveErrors(test, __FILE__, __LINE__, source, expected)

// Lowers RECURSION_LIMIT until the end of the scope.
#define SET_RECURSION_LIMIT(limit)                        \
	uint32 previous_recursion_limit = RECURSION_LIMIT;    \
	RECURSION_LIMIT = limit;                              \
	DEFER(RECURSION_LIMIT = previous_recursion_limit)

TEST(Resolver, FunctionsAndStructsCanBeUsedBeforeTheirDeclaration)
{
	CHECK_RESOLVE("function a() { b(); } function b() {}",
		"([DECLARATION]FUNCTION a ([BODY]BLOCK ([STATEMENT]CALL ([CALLEE]REFERENCE b -> FUNCTION)))) "
		"([DECLARATION]FUNCTION b ([BODY]BLOCK))");
	CHECK_RESOLVE("function f(s: S) {} struct S {}",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER s ([TYPE]REFERENCE S -> STRUCT)) ([BODY]BLOCK)) ([DECLARATION]STRUCT S)");
	CHECK_RESOLVE("function f() { f(); }", "([DECLARATION]FUNCTION f ([BODY]BLOCK ([STATEMENT]CALL ([CALLEE]REFERENCE f -> FUNCTION))))");
}

TEST(Resolver, ConstsCanOnlyBeUsedAfterTheirDeclaration)
{
	CHECK_RESOLVE("const a = 1; const b = a;", "([DECLARATION]CONST a ([VALUE]NUMBER 1)) ([DECLARATION]CONST b ([VALUE]REFERENCE a -> CONST))");
	CHECK_RESOLVE_ERRORS("const b = a; const a = 1;", "unknown identifier 'a'");
	CHECK_RESOLVE_ERRORS("const a = a;", "unknown identifier 'a'");
}

TEST(Resolver, UnknownIdentifiersStayIdentifiers)
{
	CHECK_RESOLVE_ERRORS("const a = b;", "unknown identifier 'b'");

	ZTStringView errors;
	Node* module = ParseAndResolve(test.arena, "const a = b;", &errors);
	REQUIRE(module);
	StringBuilder builder(test.arena);
	SerializeNode(builder, module->child);
	CHECK_EQ(builder.ToString(), "([DECLARATION]CONST a ([VALUE]IDENTIFIER b))");
}

TEST(Resolver, ErrorsDontStopResolving)
{
	CHECK_RESOLVE_ERRORS("function f() { a; b(c); } function g() { d; }",
		"unknown identifier 'a' | unknown identifier 'b' | unknown identifier 'c' | unknown identifier 'd'");
}

TEST(Resolver, Parameters)
{
	CHECK_RESOLVE("struct S {} function f(a: S) { return a; }",
		"([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]REFERENCE S -> STRUCT)) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]REFERENCE a -> PARAMETER))))");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S, b: S = a): S {}", "");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S = b, b: S) {}", "unknown identifier 'b'");
}

TEST(Resolver, Variables)
{
	CHECK_RESOLVE("struct S {} function f() { x: S; return x; }",
		"([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([BODY]BLOCK ([STATEMENT]VARIABLE x ([TYPE]REFERENCE S -> STRUCT)) "
		"([STATEMENT]RETURN ([VALUE]REFERENCE x -> VARIABLE))))");
	CHECK_RESOLVE("struct S {} function f(v: S) { x: S = v; }",
		"([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER v ([TYPE]REFERENCE S -> STRUCT)) "
		"([BODY]BLOCK ([STATEMENT]BINARY = ([LEFT]VARIABLE x ([TYPE]REFERENCE S -> STRUCT)) ([RIGHT]REFERENCE v -> PARAMETER))))");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { x; x: S; }", "unknown identifier 'x'");
}

TEST(Resolver, VariableNeedsAName)
{
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { a.b: S; }", "expected a name before ':'");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { 1: S; }", "expected a name before ':'");
	CHECK_RESOLVE_ERRORS("function f() { 1: T; }", "expected a name before ':' | unknown identifier 'T'");
}

TEST(Resolver, BlocksHaveTheirOwnScope)
{
	CHECK_RESOLVE_ERRORS("struct S {} function f() { { x: S; } x; }", "unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { x: S; } function g() { x; }", "unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { if a { x: S; } x; }", "unknown identifier 'a' | unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("const c = 1; function f() { { { c; } } }", "");
}

TEST(Resolver, NestedFunctions)
{
	CHECK_RESOLVE_ERRORS("function f() { g(); function g() {} }", "");
	CHECK_RESOLVE_ERRORS("function f() { function g() {} } function h() { g(); }", "unknown identifier 'g'");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { function g() { a; } }", "");
}

TEST(Resolver, Shadowing)
{
	CHECK_RESOLVE("function x() {} function f(x: S) { return x; } struct S {}",
		"([DECLARATION]FUNCTION x ([BODY]BLOCK)) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER x ([TYPE]REFERENCE S -> STRUCT)) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]REFERENCE x -> PARAMETER)))) ([DECLARATION]STRUCT S)");
	CHECK_RESOLVE("struct S {} function f(x: S) { { x: S; x; } }",
		"([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER x ([TYPE]REFERENCE S -> STRUCT)) "
		"([BODY]BLOCK ([STATEMENT]BLOCK ([STATEMENT]VARIABLE x ([TYPE]REFERENCE S -> STRUCT)) ([STATEMENT]REFERENCE x -> VARIABLE))))");
}

TEST(Resolver, AlreadyDefined)
{
	CHECK_RESOLVE_ERRORS("function a() {} function a() {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct a {} function a() {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("const a = 1; const a = 2;", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("function a() {} const a = 1;", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S, a: S) {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { x: S; x: S; }", "'x' is already defined");
	// Parameters and the function's body share a scope.
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { a: S; }", "'a' is already defined");
}

TEST(Resolver, MemberNamesAreNotResolved)
{
	CHECK_RESOLVE("struct S {} function f(a: S) { a.b.c; }",
		"([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]REFERENCE S -> STRUCT)) "
		"([BODY]BLOCK ([STATEMENT]MEMBER c ([OBJECT]MEMBER b ([OBJECT]REFERENCE a -> PARAMETER)))))");
}

TEST(Resolver, FieldTypesAreResolvedNotTheirNames)
{
	CHECK_RESOLVE_ERRORS("struct S { a: T; b: S; }", "unknown identifier 'T'");
}

TEST(Resolver, Attributes)
{
	CHECK_RESOLVE_ERRORS("@a function f() {}", "unknown identifier 'a'");
	CHECK_RESOLVE_ERRORS("struct S {} @S function f() {}", "");
	// The identifier first, then its attributes.
	CHECK_RESOLVE_ERRORS("function f(): @a(b) c {}", "unknown identifier 'c' | unknown identifier 'a' | unknown identifier 'b'");
}

TEST(Resolver, Scopes)
{
	const char* source = "function f() { { } }";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	REQUIRE(Parse(parser, &module));
	Context context;
	InitContext(context, test.arena);
	Resolver resolver = { .context = &context, .arena = test.arena, .error_arena = test.arena };
	REQUIRE(Resolve(resolver, module));

	Node* function = module->child;
	Node* body = FindChild(function, Usage::BODY);
	Node* block = body->child;
	REQUIRE(module->scope);
	CHECK(module->scope->parent == context.global_scope);
	CHECK(context.global_scope->parent == nullptr);
	CHECK(function->scope->parent == module->scope);
	CHECK(body->scope == function->scope);
	CHECK(block->scope->parent == body->scope);
	CHECK(resolver.scope == nullptr);
}

TEST(Resolver, TriangleShader)
{
	// The shader's own declarations and the built-in types resolve, only the attributes don't yet.
	CHECK_RESOLVE_ERRORS(R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

struct VSOutput
{
	position: float4;
}

function VSMain(@builtin(vertex_index) vertex_id: uint): @builtin(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)",
		"unknown identifier 'builtin' | unknown identifier 'vertex_index' | "
		"unknown identifier 'builtin' | unknown identifier 'position' | unknown identifier 'location'");
}

TEST(Resolver, BuiltInTypes)
{
	CHECK_RESOLVE("function f(a: float2, b: float3): float4 { x: float; }",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]TYPE_REFERENCE float2 -> float2)) "
		"([PARAMETER]PARAMETER b ([TYPE]TYPE_REFERENCE float3 -> float3)) ([RETURN_TYPE]TYPE_REFERENCE float4 -> float4) "
		"([BODY]BLOCK ([STATEMENT]VARIABLE x ([TYPE]TYPE_REFERENCE float -> float))))");
	CHECK_RESOLVE("function f(): void { return float4(1.0); }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]TYPE_REFERENCE void -> void) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]CALL ([CALLEE]TYPE_REFERENCE float4 -> float4) ([ARGUMENT]NUMBER 1.0)))))");
	CHECK_RESOLVE_ERRORS("function f(a: int, b: uint) {}", "");
	CHECK_RESOLVE_ERRORS("function f(a: bool, b: float5) {}", "unknown identifier 'bool' | unknown identifier 'float5'");
}

TEST(Resolver, BuiltInTypesCanBeShadowed)
{
	// The module's scope is under the global one.
	CHECK_RESOLVE("struct float2 {} function f(a: float2) {}",
		"([DECLARATION]STRUCT float2) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]REFERENCE float2 -> STRUCT)) ([BODY]BLOCK))");
}

TEST(Resolver, RecursionLimit)
{
	// Operator chains are built without recursion in the parser, so the resolver is the first to see how deep they go.
	// module, const, 5 '!', a: 8 levels.
	SET_RECURSION_LIMIT(8);
	CHECK_RESOLVE_ERRORS("const a = 1; const x = !!!!!a;", "");
	CHECK_RESOLVE_ERRORS("const a = 1; const x = !!!!!!a;", "nested too deeply");
	CHECK_RESOLVE_ERRORS("const a = 1; const x = a + a + a + a + a + a;", "");
	CHECK_RESOLVE_ERRORS("const a = 1; const x = a + a + a + a + a + a + a;", "nested too deeply | nested too deeply");
}

TEST(Resolver, DefaultRecursionLimitStopsDeepChains)
{
	StringBuilder builder(test.arena);
	builder.Append("const a = 1; const x = ");
	for (int i = 0; i < 10000; ++i)
		builder.Append("!");
	builder.Append("a;");
	CHECK_RESOLVE_ERRORS(builder.ToString().CString(), "nested too deeply");
}
