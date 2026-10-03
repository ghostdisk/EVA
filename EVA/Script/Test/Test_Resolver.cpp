#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>

using namespace EVA;
using namespace EVA::Script;

// Parses and resolves source. Returns the module, or nullptr with a "parse error: <message>" in out_errors.
// Resolve errors go to out_errors joined with " | ", empty if there were none.
static Node* ParseAndResolve(Arena* arena, ContextKind kind, const char* source, ZTStringView* out_errors)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = arena, .error_arena = arena };
	Node* module = nullptr;
	if (!Parse(parser, &module))
	{
		*out_errors = aprintf(arena, "parse error: %s", parser.errors.back()->message.CString());
		return nullptr;
	}

	Context context;
	InitContext(context, arena, kind); // in arena, so the types outlive this function along with the tree referencing them
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
static void CheckResolve(Test::Context& test, const char* file, int line, ContextKind kind, const char* source, StringView expected)
{
	ZTStringView errors;
	Node* module = ParseAndResolve(test.arena, kind, source, &errors);
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
		SnapshotNodeToString(builder, declaration);
	}
	if (builder.ToString() == expected)
		return;
	Test::ReportFailure(test, file, line, "resolving \"%s\"\n    got      %s\n    expected %.*s", source, builder.ToString().CString(),
		(int)expected.length, (const char*)expected.data);
}

// Source must resolve with exactly these errors, joined with " | ". "" for none.
static void CheckResolveErrors(Test::Context& test, const char* file, int line, ContextKind kind, const char* source, StringView expected)
{
	ZTStringView errors;
	ParseAndResolve(test.arena, kind, source, &errors);
	if (errors == expected)
		return;
	Test::ReportFailure(test, file, line, "resolving \"%s\"\n    got errors      %s\n    expected errors %.*s", source, errors.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_RESOLVE(source, expected) CheckResolve(test, __FILE__, __LINE__, ContextKind::SCRIPT, source, expected)
#define CHECK_RESOLVE_ERRORS(source, expected) CheckResolveErrors(test, __FILE__, __LINE__, ContextKind::SCRIPT, source, expected)
#define CHECK_SHADER_RESOLVE(source, expected) CheckResolve(test, __FILE__, __LINE__, ContextKind::SHADER, source, expected)
#define CHECK_SHADER_RESOLVE_ERRORS(source, expected) CheckResolveErrors(test, __FILE__, __LINE__, ContextKind::SHADER, source, expected)

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
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER s ([DECLARED_TYPE]REFERENCE S -> TYPE S)) ([BODY]BLOCK)) ([DECLARATION]STRUCT:S S)");
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
	Node* module = ParseAndResolve(test.arena, ContextKind::SCRIPT, "const a = b;", &errors);
	REQUIRE(module);
	StringBuilder builder(test.arena);
	SnapshotNodeToString(builder, module->child);
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
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]REFERENCE a -> PARAMETER))))");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S, b: S = a): S {}", "");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S = b, b: S) {}", "unknown identifier 'b'");
}

TEST(Resolver, Variables)
{
	CHECK_RESOLVE("struct S {} function f() { let x: S; return x; }",
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([BODY]BLOCK ([STATEMENT]VARIABLE x ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
		"([STATEMENT]RETURN ([VALUE]REFERENCE x -> VARIABLE))))");
	CHECK_RESOLVE("struct S {} function f(v: S) { let x: S = v; }",
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER v ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
		"([BODY]BLOCK ([STATEMENT]VARIABLE x ([DECLARED_TYPE]REFERENCE S -> TYPE S) ([VALUE]REFERENCE v -> PARAMETER))))");
	// The value is resolved before the variable is declared, so it sees an outer x.
	CHECK_RESOLVE("function f(x: int) { { let x = x; } }",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER x ([DECLARED_TYPE]REFERENCE int -> TYPE int)) "
		"([BODY]BLOCK ([STATEMENT]BLOCK ([STATEMENT]VARIABLE x ([VALUE]REFERENCE x -> PARAMETER)))))");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { x; let x: S; }", "unknown identifier 'x'");
	// The attributes stay on the variable.
	CHECK_RESOLVE("struct S {} function f() { let @S x: S; }",
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([BODY]BLOCK ([STATEMENT]VARIABLE x ([ATTRIBUTE]REFERENCE S -> TYPE S) "
		"([DECLARED_TYPE]REFERENCE S -> TYPE S))))");
}

TEST(Resolver, Globals)
{
	// Declared ahead, so any function can use them.
	CHECK_RESOLVE("function f(): int { return g; } let g = 1;",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE int -> TYPE int) ([BODY]BLOCK ([STATEMENT]RETURN "
		"([VALUE]REFERENCE g -> VARIABLE)))) ([DECLARATION]VARIABLE g ([VALUE]NUMBER 1))");
	CHECK_RESOLVE_ERRORS("let g = 1; let g = 2;", "'g' is already defined");
	CHECK_RESOLVE_ERRORS("let g = 1; function g() {}", "'g' is already defined");
	// Unlike locals, a function can use the module's.
	CHECK_RESOLVE_ERRORS("let g = 1; function f() { function h(): int { return g; } }", "");
}

TEST(Resolver, BlocksHaveTheirOwnScope)
{
	CHECK_RESOLVE_ERRORS("struct S {} function f() { { let x: S; } x; }", "unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { let x: S; } function g() { x; }", "unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { if a { let x: S; } x; }", "unknown identifier 'a' | unknown identifier 'x'");
	CHECK_RESOLVE_ERRORS("const c = 1; function f() { { { c; } } }", "");
}

TEST(Resolver, NestedFunctions)
{
	CHECK_RESOLVE_ERRORS("function f() { g(); function g() {} }", "");
	CHECK_RESOLVE_ERRORS("function f() { function g() {} } function h() { g(); }", "unknown identifier 'g'");
}

TEST(Resolver, Shadowing)
{
	CHECK_RESOLVE("function x() {} function f(x: S) { return x; } struct S {}",
		"([DECLARATION]FUNCTION x ([BODY]BLOCK)) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER x ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]REFERENCE x -> PARAMETER)))) ([DECLARATION]STRUCT:S S)");
	CHECK_RESOLVE("struct S {} function f(x: S) { { let x: S; x; } }",
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER x ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
		"([BODY]BLOCK ([STATEMENT]BLOCK ([STATEMENT]VARIABLE x ([DECLARED_TYPE]REFERENCE S -> TYPE S)) ([STATEMENT]REFERENCE x -> VARIABLE))))");
}

TEST(Resolver, AlreadyDefined)
{
	CHECK_RESOLVE_ERRORS("function a() {} function a() {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct a {} function a() {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("const a = 1; const a = 2;", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("function a() {} const a = 1;", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S, a: S) {}", "'a' is already defined");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { let x: S; let x: S; }", "'x' is already defined");
	// Parameters and the function's body share a scope.
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { let a: S; }", "'a' is already defined");
}

TEST(Resolver, MemberNamesAreNotResolved)
{
	CHECK_RESOLVE("struct S {} function f(a: S) { a.b.c; }",
		"([DECLARATION]STRUCT:S S) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([DECLARED_TYPE]REFERENCE S -> TYPE S)) "
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
	InitContext(context, test.arena, ContextKind::SCRIPT);
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
	CHECK_EQ(context.global_scope->kind, ScopeKind::GLOBAL);
	CHECK_EQ(module->scope->kind, ScopeKind::MODULE);
	CHECK_EQ(function->scope->kind, ScopeKind::FUNCTION);
	CHECK_EQ(block->scope->kind, ScopeKind::BLOCK);
}

TEST(Resolver, NoCapturing)
{
	// A nested function can't use the parameters and variables of the functions around it.
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { function g() { a; } }",
		"'a' belongs to an enclosing function, capturing isn't supported yet");
	CHECK_RESOLVE_ERRORS("struct S {} function f() { { let x: S; function g(): S { return x; } } }",
		"'x' belongs to an enclosing function, capturing isn't supported yet");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { function g() { function h() { a; } } }",
		"'a' belongs to an enclosing function, capturing isn't supported yet");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { function g(b: [a]float) {} }",
		"'a' belongs to an enclosing function, capturing isn't supported yet");

	// Its own, those of blocks in the same function, and everything else around it are fine.
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { { { a; } } }", "");
	CHECK_RESOLVE_ERRORS("struct S {} function f(a: S) { function g(a: S) { a; } }", "");
	CHECK_RESOLVE_ERRORS("function f() { const c = 1; struct T {} function h() {} function g() { c; h; let x: T; } }", "");
}

TEST(Resolver, ShaderIntrinsics)
{
	CHECK_SHADER_RESOLVE("@entry(vertex) function f() {}",
		"([DECLARATION]FUNCTION f ([ATTRIBUTE]CALL ([CALLEE]REFERENCE entry -> INTRINSIC entry) "
		"([ARGUMENT]REFERENCE vertex -> ENUM_VALUE)) ([BODY]BLOCK))");
	// Stage names are only visible in entry's argument, semantic names only in semantic's.
	CHECK_SHADER_RESOLVE_ERRORS("@entry(position) function f() {}", "unknown identifier 'position'");
	CHECK_SHADER_RESOLVE_ERRORS("function f(@semantic(vertex) a: uint) {}", "unknown identifier 'vertex'");
	CHECK_SHADER_RESOLVE_ERRORS("const v = vertex;", "unknown identifier 'vertex'");
	CHECK_SHADER_RESOLVE("function f(): @location(0) float4 {}",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE float4 -> TYPE float4 "
		"([ATTRIBUTE]CALL ([CALLEE]REFERENCE location -> INTRINSIC location) ([ARGUMENT]NUMBER 0))) ([BODY]BLOCK))");
	CHECK_SHADER_RESOLVE_ERRORS("@entry(fragment) function f(@semantic(x) a: uint) {}", "unknown identifier 'x'");
}

TEST(Resolver, SemanticArgumentsAreSemanticNames)
{
	CHECK_SHADER_RESOLVE("function f(@semantic(vertex_index) id: uint) {}",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER id ([ATTRIBUTE]CALL ([CALLEE]REFERENCE semantic -> INTRINSIC semantic) "
		"([ARGUMENT]REFERENCE vertex_index -> ENUM_VALUE)) ([DECLARED_TYPE]REFERENCE uint -> TYPE uint)) ([BODY]BLOCK))");
	// Nothing else is visible, and the semantic names win over the module's.
	CHECK_SHADER_RESOLVE_ERRORS("const c = 1; function f(@semantic(c) a: uint) {}", "unknown identifier 'c'");
	CHECK_SHADER_RESOLVE_ERRORS("function f(@semantic(float4) a: uint) {}", "unknown identifier 'float4'");
	CHECK_SHADER_RESOLVE("function f(position: float4): @semantic(position) float4 {}",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER position ([DECLARED_TYPE]REFERENCE float4 -> TYPE float4)) "
		"([RETURN_TYPE]REFERENCE float4 -> TYPE float4 ([ATTRIBUTE]CALL ([CALLEE]REFERENCE semantic -> INTRINSIC semantic) "
		"([ARGUMENT]REFERENCE position -> ENUM_VALUE))) ([BODY]BLOCK))");
	// Semantic names are only visible in the arguments.
	CHECK_SHADER_RESOLVE_ERRORS("function f(@semantic(position) a: float4) { position; }", "unknown identifier 'position'");
	CHECK_SHADER_RESOLVE_ERRORS("const x = 1; function f(@@x semantic(position) a: float4) {}", "");
}

TEST(Resolver, OtherIntrinsicArgumentsResolveNormally)
{
	CHECK_SHADER_RESOLVE("const SLOT = 0; function f(): @location(SLOT) float4 {}",
		"([DECLARATION]CONST SLOT ([VALUE]NUMBER 0)) ([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE float4 -> TYPE float4 "
		"([ATTRIBUTE]CALL ([CALLEE]REFERENCE location -> INTRINSIC location) ([ARGUMENT]REFERENCE SLOT -> CONST))) ([BODY]BLOCK))");
	CHECK_SHADER_RESOLVE_ERRORS("function f(): @location(position) float4 {}", "unknown identifier 'position'");
}

TEST(Resolver, ShaderIntrinsicsAreNotInScripts)
{
	CHECK_RESOLVE_ERRORS("@entry(vertex) function f(): @location(0) float4 {}", "unknown identifier 'entry' | unknown identifier 'vertex' | unknown identifier 'location'");
}

TEST(Resolver, ShaderIntrinsicsCanBeShadowed)
{
	CHECK_SHADER_RESOLVE("const location = 1; function f(): @location(0) float4 {}",
		"([DECLARATION]CONST location ([VALUE]NUMBER 1)) ([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE float4 -> TYPE float4 "
		"([ATTRIBUTE]CALL ([CALLEE]REFERENCE location -> CONST) ([ARGUMENT]NUMBER 0))) ([BODY]BLOCK))");
}

TEST(Resolver, BuiltInConstants)
{
	Context context;
	InitContext(context, test.arena, ContextKind::SCRIPT);
	Constant* constant = test.arena->New<Constant>();
	constant->type = (Type*)context.global_scope->first->element;
	Definition* definition = test.arena->New<Definition>();
	definition->name = GetAtom("answer");
	definition->element = constant;
	definition->next = context.global_scope->first;
	context.global_scope->first = definition;

	const char* source = "const a = answer;";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	REQUIRE(Parse(parser, &module));
	Resolver resolver = { .context = &context, .arena = test.arena, .error_arena = test.arena };
	REQUIRE(Resolve(resolver, module));

	Node* value = FindChild(module->child, Usage::VALUE);
	CHECK_EQ(value->node_type, NodeType::REFERENCE);
	CHECK(value->target == constant);
}

TEST(Resolver, BuiltInTypes)
{
	CHECK_RESOLVE("function f(a: float2, b: float3): float4 { let x: float; }",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([DECLARED_TYPE]REFERENCE float2 -> TYPE float2)) "
		"([PARAMETER]PARAMETER b ([DECLARED_TYPE]REFERENCE float3 -> TYPE float3)) ([RETURN_TYPE]REFERENCE float4 -> TYPE float4) "
		"([BODY]BLOCK ([STATEMENT]VARIABLE x ([DECLARED_TYPE]REFERENCE float -> TYPE float))))");
	CHECK_RESOLVE("function f(): void { return float4(1.0); }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE void -> TYPE void) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]CALL ([CALLEE]REFERENCE float4 -> TYPE float4) ([ARGUMENT]NUMBER 1.0)))))");
	CHECK_RESOLVE_ERRORS("function f(a: int, b: uint) {}", "");
	CHECK_RESOLVE_ERRORS("function f(a: bool, b: float5) {}", "unknown identifier 'bool' | unknown identifier 'float5'");
}

TEST(Resolver, BuiltInTypesCanBeShadowed)
{
	// The module's scope is under the global one.
	CHECK_RESOLVE("struct float2 {} function f(a: float2) {}",
		"([DECLARATION]STRUCT:float2 float2) ([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([DECLARED_TYPE]REFERENCE float2 -> TYPE float2)) ([BODY]BLOCK))");
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
