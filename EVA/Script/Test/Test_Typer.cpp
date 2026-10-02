#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>
#include <string.h>

using namespace EVA;
using namespace EVA::Script;

// Parses, resolves and types source. Returns the module, or nullptr with a "parse error: ..." or "resolve error: ..." in
// out_errors. Type errors go to out_errors joined with " | ", empty if there were none.
static Node* ParseResolveAndType(Arena* arena, ContextKind kind, const char* source, ZTStringView* out_errors)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = arena, .error_arena = arena };
	Node* module = nullptr;
	if (!Parse(parser, &module))
	{
		*out_errors = aprintf(arena, "parse error: %s", parser.errors.back()->message.CString());
		return nullptr;
	}

	Context* context = arena->New<Context>(); // outlives this function along with the tree referencing its types
	InitContext(*context, arena, kind);
	Resolver resolver = { .context = context, .arena = arena, .error_arena = arena };
	if (!Resolve(resolver, module))
	{
		*out_errors = aprintf(arena, "resolve error: %s", resolver.errors[0]->message.CString());
		return nullptr;
	}

	Typer typer = { .context = context, .arena = arena, .error_arena = arena };
	TypeCheck(typer, module);

	StringBuilder builder(arena);
	for (size_t i = 0; i < typer.errors.size(); ++i)
	{
		if (i)
			builder.Append(" | ");
		builder.Append(typer.errors[i]->message);
	}
	*out_errors = builder.ToString();
	return module;
}

// Source must type without errors to the expected declarations, serialized like the parser tests.
static void CheckType(Test::Context& test, const char* file, int line, ContextKind kind, const char* source, StringView expected)
{
	ZTStringView errors;
	Node* module = ParseResolveAndType(test.arena, kind, source, &errors);
	if (!module || errors.length)
	{
		Test::ReportFailure(test, file, line, "typing \"%s\"\n    failed with %s", source, errors.CString());
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
	Test::ReportFailure(test, file, line, "typing \"%s\"\n    got      %s\n    expected %.*s", source, builder.ToString().CString(),
		(int)expected.length, (const char*)expected.data);
}

// Source must type with exactly these errors, joined with " | ". "" for none.
static void CheckTypeErrors(Test::Context& test, const char* file, int line, ContextKind kind, const char* source, StringView expected)
{
	ZTStringView errors;
	ParseResolveAndType(test.arena, kind, source, &errors);
	if (errors == expected)
		return;
	Test::ReportFailure(test, file, line, "typing \"%s\"\n    got errors      %s\n    expected errors %.*s", source, errors.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_TYPE(source, expected) CheckType(test, __FILE__, __LINE__, ContextKind::SCRIPT, source, expected)
#define CHECK_TYPE_ERRORS(source, expected) CheckTypeErrors(test, __FILE__, __LINE__, ContextKind::SCRIPT, source, expected)
#define CHECK_SHADER_TYPE_ERRORS(source, expected) CheckTypeErrors(test, __FILE__, __LINE__, ContextKind::SHADER, source, expected)

// The last declaration in source, which must type without errors.
static Node* LastDeclaration(Test::Context& test, const char* source)
{
	ZTStringView errors;
	Node* module = ParseResolveAndType(test.arena, ContextKind::SCRIPT, source, &errors);
	if (!module || errors.length)
		return nullptr;
	Node* last = module->child;
	while (last && last->next)
		last = last->next;
	return last;
}

// The value of the last const in source as 32-bit components.
static bool ConstantBits(Test::Context& test, const char* source, uint32* out_bits, uint32 count)
{
	Node* node = LastDeclaration(test, source);
	if (!node || node->node_type != NodeType::CONST)
		return false;
	Constant* constant = FindChild(node, Usage::VALUE)->constant;
	if (constant->bytes.count != count * 4)
		return false;
	memcpy(out_bits, constant->bytes.data, (size_t)count * 4);
	return true;
}

static uint32 Bits(float value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	return bits;
}

TEST(Typer, Numbers)
{
	// A const's value is evaluated and folded into a CONSTANT.
	CHECK_TYPE("const a = 1;", "([DECLARATION]CONST:int a ([VALUE]CONSTANT:int 1))");
	CHECK_TYPE("const a = 1.5;", "([DECLARATION]CONST:float a ([VALUE]CONSTANT:float 1.5))");
	CHECK_TYPE("const a = 1e3;", "([DECLARATION]CONST:float a ([VALUE]CONSTANT:float 1000.0))");
	CHECK_TYPE("const a: uint = 1;", "([DECLARATION]CONST:uint a ([DECLARED_TYPE]REFERENCE:uint uint -> TYPE uint) ([VALUE]CONSTANT:uint 1))");
	// Literals take the expected type.
	CHECK_TYPE("const a: float = 1;", "([DECLARATION]CONST:float a ([DECLARED_TYPE]REFERENCE:float float -> TYPE float) ([VALUE]CONSTANT:float 1.0))");
	CHECK_TYPE("function f(): uint { return 1; }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE:uint uint -> TYPE uint) ([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]NUMBER:uint 1))))");
	CHECK_TYPE_ERRORS("function f(): int { return 1.5; }", "'1.5' is not an integer");

	CHECK_TYPE_ERRORS("const a: int = 1.5;", "'1.5' is not an integer");
	CHECK_TYPE_ERRORS("const a: int = 2147483647;", "");
	CHECK_TYPE_ERRORS("const a: int = 2147483648;", "'2147483648' is out of range for int");
	CHECK_TYPE_ERRORS("const a: uint = 4294967295;", "");
	CHECK_TYPE_ERRORS("const a: uint = 4294967296;", "'4294967296' is out of range for uint");
	CHECK_TYPE_ERRORS("const a = 1e40;", "'1e+40' is out of range for float");

	// Integers become floats only when they fit exactly.
	CHECK_TYPE_ERRORS("const a: float = 16777216;", "");
	CHECK_TYPE_ERRORS("const a: float = 16777217;", "'16777217' can't be represented exactly as a float");
	CHECK_TYPE_ERRORS("const a: float = 0x40000000;", "");
	CHECK_TYPE_ERRORS("const a: float = 18446744073709551615;", "'18446744073709551615' can't be represented exactly as a float");

	uint32 bits[1];
	REQUIRE(ConstantBits(test, "const a = 0x1F;", bits, 1));
	CHECK_EQ(bits[0], 31u);
	REQUIRE(ConstantBits(test, "const a = 0.5;", bits, 1));
	CHECK_EQ(bits[0], Bits(0.5f));
	REQUIRE(ConstantBits(test, "const a: float = 0x40000000;", bits, 1));
	CHECK_EQ(bits[0], Bits(1073741824.0f));
	// Parsed as a float directly, not rounded from a double.
	REQUIRE(ConstantBits(test, "const a = 0.1;", bits, 1));
	CHECK_EQ(bits[0], Bits(0.1f));
}

TEST(Typer, Unary)
{
	CHECK_TYPE("const a = -1;", "([DECLARATION]CONST:int a ([VALUE]CONSTANT:int -1))");
	CHECK_TYPE("function f(): int { return -1; }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE:int int -> TYPE int) ([BODY]BLOCK ([STATEMENT]RETURN "
		"([VALUE]UNARY:int - ([OPERAND]NUMBER:int 1)))))");
	CHECK_TYPE_ERRORS("function f(a: uint): uint { return -a; }", "can't apply '-' to uint");
	CHECK_TYPE_ERRORS("const a: uint = -1;", "can't apply '-' to uint");
	CHECK_TYPE_ERRORS("const a = ~1.0;", "can't apply '~' to float");
	CHECK_TYPE_ERRORS("const a = !1;", "'!' isn't supported yet");

	uint32 bits[1];
	REQUIRE(ConstantBits(test, "const a = -5;", bits, 1));
	CHECK_EQ((int32)bits[0], -5);
	REQUIRE(ConstantBits(test, "const a = -0.5;", bits, 1));
	CHECK_EQ(bits[0], Bits(-0.5f));
	REQUIRE(ConstantBits(test, "const a: uint = ~0;", bits, 1));
	CHECK_EQ(bits[0], 0xFFFFFFFFu);
}

TEST(Typer, Binary)
{
	CHECK_TYPE("const a = 1 + 2;", "([DECLARATION]CONST:int a ([VALUE]CONSTANT:int 3))");
	// A literal takes the other side's type, whichever side it's on.
	CHECK_TYPE("const a = 1 + 2.0;", "([DECLARATION]CONST:float a ([VALUE]CONSTANT:float 3.0))");
	CHECK_TYPE("function f(x: float): float { return 2 * x; }",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER:float x ([DECLARED_TYPE]REFERENCE:float float -> TYPE float)) "
		"([RETURN_TYPE]REFERENCE:float float -> TYPE float) ([BODY]BLOCK ([STATEMENT]RETURN "
		"([VALUE]BINARY:float * ([LEFT]NUMBER:float 2) ([RIGHT]REFERENCE:float x -> PARAMETER)))))");
	CHECK_TYPE_ERRORS("function f(a: int, b: uint): int { return a + b; }", "mismatched types int and uint");

	CHECK_TYPE_ERRORS("const a: int = 1; const b: uint = 2; const c = a + b;", "mismatched types int and uint");
	CHECK_TYPE_ERRORS("const a = 1 == 2;", "'==' isn't supported yet");
	CHECK_TYPE_ERRORS("const a = 7 / 0;", "division by zero");
	CHECK_TYPE_ERRORS("const a = 7 % 0;", "division by zero");
	CHECK_TYPE_ERRORS("const a = -2147483647 - 1; const b = a / -1;", "integer overflow");

	uint32 bits[1];
	REQUIRE(ConstantBits(test, "const a = 2 + 3 * 4;", bits, 1));
	CHECK_EQ(bits[0], 14u);
	REQUIRE(ConstantBits(test, "const a = -7 / 2;", bits, 1));
	CHECK_EQ((int32)bits[0], -3);
	REQUIRE(ConstantBits(test, "const a: uint = 7 % 4;", bits, 1));
	CHECK_EQ(bits[0], 3u);
	REQUIRE(ConstantBits(test, "const a = 1.0 / 4;", bits, 1));
	CHECK_EQ(bits[0], Bits(0.25f));
	REQUIRE(ConstantBits(test, "const a = 2147483647 + 1;", bits, 1)); // wraps
	CHECK_EQ((int32)bits[0], INT32_MIN);
}

TEST(Typer, VectorConstructors)
{
	CHECK_TYPE("const v = float2(1.0, 2.0);", "([DECLARATION]CONST:float2 v ([VALUE]CONSTANT:float2 (1.0, 2.0)))");
	CHECK_TYPE("function f(): float2 { return float2(1.0, 2.0); }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE:float2 float2 -> TYPE float2) ([BODY]BLOCK ([STATEMENT]RETURN "
		"([VALUE]CALL:float2 ([CALLEE]REFERENCE:float2 float2 -> TYPE float2) ([ARGUMENT]NUMBER:float 1.0) ([ARGUMENT]NUMBER:float 2.0)))))");
	CHECK_TYPE_ERRORS("const v = float4(1, 2, 3, 4);", "");
	CHECK_TYPE_ERRORS("const v = float4(float2(1.0, 2.0), 3.0, 4.0);", "");
	CHECK_TYPE_ERRORS("const v = float4(1.0);", "");
	CHECK_TYPE_ERRORS("const v = float4(1.0, 2.0);", "float4 needs 4 components, got 2");
	CHECK_TYPE_ERRORS("const v = float2(float2(1.0, 2.0), 3.0);", "float2 needs 2 components, got 3");
	// Evaluating a constant stops at the first error, typing code goes on.
	CHECK_TYPE_ERRORS("const i = 1; const v = float2(i, i);", "can't construct float2 from int");
	CHECK_TYPE_ERRORS("function f(i: int): float2 { return float2(i, i); }", "can't construct float2 from int | can't construct float2 from int");
	CHECK_TYPE_ERRORS("const v = int(1);", "constructing int isn't supported yet");

	uint32 bits[4];
	REQUIRE(ConstantBits(test, "const v = float4(float2(1.0, 2.0), 3, 4.0);", bits, 4));
	CHECK_EQ(bits[0], Bits(1.0f));
	CHECK_EQ(bits[1], Bits(2.0f));
	CHECK_EQ(bits[2], Bits(3.0f));
	CHECK_EQ(bits[3], Bits(4.0f));
	REQUIRE(ConstantBits(test, "const v = float4(0.5);", bits, 4));
	for (uint32 i = 0; i < 4; ++i)
		CHECK_EQ(bits[i], Bits(0.5f));
	REQUIRE(ConstantBits(test, "const v = -float2(1.0, 2.0) * float2(3.0, 0.5);", bits, 2));
	CHECK_EQ(bits[0], Bits(-3.0f));
	CHECK_EQ(bits[1], Bits(-1.0f));
}

TEST(Typer, Arrays)
{
	CHECK_TYPE("const a: [2]float = { 1.0, 2.0 };",
		"([DECLARATION]CONST:[2]float a ([DECLARED_TYPE]ARRAY_TYPE:[2]float ([SIZE]CONSTANT:uint 2) "
		"([ELEMENT]REFERENCE:float float -> TYPE float)) ([VALUE]CONSTANT:[2]float {1.0, 2.0}))");
	CHECK_TYPE_ERRORS("const n = 2; const a: [n]float = { 1.0, 2.0 };", "");
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0 };", "[2]float needs 2 elements, got 1");
	CHECK_TYPE_ERRORS("const a: [0]float = {};", "array size must be at least 1, got 0");
	CHECK_TYPE_ERRORS("const a: [1.0]float = { 1.0 };", "'1.0' is not an integer");
	CHECK_TYPE_ERRORS("const n = 1.0; const a: [n]float = { 1.0 };", "array size must be an int or uint, got float");
	CHECK_TYPE_ERRORS("const a: [4294967295][4294967295]float4 = {};", "[4294967295]float4 is too large");
	CHECK_TYPE_ERRORS("function f(n: uint, a: [n]float) {}", "array size must be a constant");
	CHECK_TYPE_ERRORS("const a = { 1.0 };", "can't tell the type of an initializer list here");
	CHECK_TYPE_ERRORS("const a: float = { 1.0 };", "can't initialize float with an initializer list");

	// Array types are unique.
	ZTStringView errors;
	Node* module = ParseResolveAndType(test.arena, ContextKind::SCRIPT,
		"const a: [3]float2 = { float2(1.0), float2(2.0), float2(3.0) }; const b: [3]float2 = a;", &errors);
	REQUIRE(module && !errors.length);
	Node* a = module->child;
	Node* node = a->next;
	CHECK(a->type == node->type);
	CHECK_EQ(node->type->type_kind, TypeKind::ARRAY);
	ArrayType* array = (ArrayType*)node->type;
	CHECK_EQ(array->length, 3u);
	CHECK_EQ(array->stride, 8u);
	CHECK_EQ(array->size, 24u);

	uint32 bits[6];
	REQUIRE(ConstantBits(test, "const a: [3]float2 = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };", bits, 6));
	float expected[] = { 0.0f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
	for (uint32 i = 0; i < 6; ++i)
		CHECK_EQ(bits[i], Bits(expected[i]));
}

TEST(Typer, Indexing)
{
	CHECK_TYPE("const a: [2]float = { 1.0, 2.0 }; function f(i: uint): float { return a[i]; }",
		"([DECLARATION]CONST:[2]float a ([DECLARED_TYPE]ARRAY_TYPE:[2]float ([SIZE]CONSTANT:uint 2) "
		"([ELEMENT]REFERENCE:float float -> TYPE float)) ([VALUE]CONSTANT:[2]float {1.0, 2.0})) "
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER:uint i ([DECLARED_TYPE]REFERENCE:uint uint -> TYPE uint)) "
		"([RETURN_TYPE]REFERENCE:float float -> TYPE float) ([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]INDEX:float "
		"([OBJECT]REFERENCE:[2]float a -> CONST) ([INDEX]REFERENCE:uint i -> PARAMETER)))))");
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0, 2.0 }; const b = a[1];", "");
	uint32 bits[1];
	REQUIRE(ConstantBits(test, "const a: [2]float = { 1.0, 2.0 }; const b = a[1];", bits, 1));
	CHECK_EQ(bits[0], Bits(2.0f));
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0, 2.0 }; const b = a[2];", "index 2 is out of bounds for [2]float");
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0, 2.0 }; const b = a[-1];", "can't apply '-' to uint");
	CHECK_TYPE_ERRORS("const i = -1; const a: [2]float = { 1.0, 2.0 }; const b = a[i];", "index -1 is out of bounds for [2]float");
	CHECK_TYPE_ERRORS("const x = 1.0; const a: [2]float = { 1.0, 2.0 }; const b = a[x];", "index must be an int or uint, got float");
	CHECK_TYPE_ERRORS("const a = 1.0; const b = a[0];", "can't index float");
}

TEST(Typer, Structs)
{
	CHECK_TYPE("struct S { a: float; b: float4; }",
		"([DECLARATION]STRUCT:S S ([MEMBER]FIELD:float a ([DECLARED_TYPE]REFERENCE:float float -> TYPE float)) "
		"([MEMBER]FIELD:float4 b ([DECLARED_TYPE]REFERENCE:float4 float4 -> TYPE float4)))");

	Node* node = LastDeclaration(test, "struct S { a: float; b: float4; c: [2]float2; }");
	REQUIRE(node);
	StructType* type = (StructType*)node->type;
	CHECK_EQ(type->state, StructState::COMPLETE);
	REQUIRE(type->fields.count == 3);
	CHECK_EQ(type->fields[0].offset, 0u);
	CHECK_EQ(type->fields[1].offset, 4u);
	CHECK_EQ(type->fields[2].offset, 20u);
	CHECK_EQ(type->size, 36u);
	CHECK_EQ(type->alignment, 4u);

	// Fields are typed when they're first needed, so a struct can be used before its declaration.
	CHECK_TYPE_ERRORS("function f(s: S): float { return s.a; } struct S { a: float; }", "");
	CHECK_TYPE_ERRORS("struct A { b: B; } struct B { a: float; }", "");
	CHECK_TYPE_ERRORS("struct S { a: float; } function f(s: S): float { return s.b; }", "S has no member 'b'");
	CHECK_TYPE_ERRORS("const a = 1.0; const b = a.x;", "float has no member 'x'");
	CHECK_TYPE_ERRORS("struct S { s: S; }", "'S' contains itself");
	CHECK_TYPE_ERRORS("struct A { b: B; } struct B { a: A; }", "'A' contains itself");
	CHECK_TYPE_ERRORS("struct S { a: float = 1.0; }", "default values aren't supported yet");

	uint32 bits[5];
	REQUIRE(ConstantBits(test, "struct S { a: float; b: float4; } const s: S = { 1.0, float4(2.0) };", bits, 5));
	CHECK_EQ(bits[0], Bits(1.0f));
	CHECK_EQ(bits[4], Bits(2.0f));
	CHECK_TYPE_ERRORS("struct S { a: float; b: float4; } const s: S = { 1.0 };", "S needs 2 elements, got 1");
	CHECK_TYPE_ERRORS("struct S { a: float; } const s: S = { 1 };", "");
	CHECK_TYPE_ERRORS("struct S { a: float; } const s: S = { float2(1.0) };", "expected float, got float2");
}

TEST(Typer, Functions)
{
	CHECK_TYPE("function f(): float { x: float; return x; }",
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE:float float -> TYPE float) ([BODY]BLOCK "
		"([STATEMENT]VARIABLE:float x ([DECLARED_TYPE]REFERENCE:float float -> TYPE float)) "
		"([STATEMENT]RETURN ([VALUE]REFERENCE:float x -> VARIABLE))))");
	CHECK_TYPE_ERRORS("function f(): float { return 1; }", "");
	CHECK_TYPE_ERRORS("function f() { return; }", "");
	CHECK_TYPE_ERRORS("function f(): void { return; }", "");
	CHECK_TYPE_ERRORS("function f(): float { return; }", "'return' needs a value of type float");
	CHECK_TYPE_ERRORS("function f() { return 1; }", "a function returning void can't return a value");
	CHECK_TYPE_ERRORS("function f(): uint { x: int; return x; }", "expected uint, got int");
	CHECK_TYPE_ERRORS("function f(a: int = 1) {}", "default values aren't supported yet");
	CHECK_TYPE_ERRORS("function g() {} function f() { g(); }", "calling functions isn't supported yet");
	CHECK_TYPE_ERRORS("function g() {} function f(): int { return g; }", "'g' is a function, which can only be called");
	CHECK_TYPE_ERRORS("function f(a: int): int { return a(); }", "int can't be called");
	CHECK_TYPE_ERRORS("function f(): int { return (1)(2); }", "int can't be called");
	CHECK_TYPE_ERRORS("function f(): float2 { return float2(1.0)(2.0); }", "float2 can't be called");
	CHECK_TYPE_ERRORS("function f(a: int, b: uint): int { return (a + b)(1); }", "mismatched types int and uint");
	CHECK_TYPE_ERRORS("function g() {} const a = g;", "a const's value must be a constant");
	CHECK_TYPE_ERRORS("const a = 1; const b = a();", "a const's value must be a constant");
	// Nested functions have their own return type.
	CHECK_TYPE_ERRORS("function f(): float { function g(): int { return 1; } return 1.0; }", "");
}

TEST(Typer, TypesAndValues)
{
	CHECK_TYPE_ERRORS("const a = float4;", "'float4' is a type, not a value");
	CHECK_TYPE_ERRORS("const a = 1; const b: a = 1;", "expected a type");
	CHECK_TYPE_ERRORS("const a = [2]float;", "expected a value, got a type");
	CHECK_TYPE_ERRORS("function f(): int { return true; }", "bool isn't supported yet");
	CHECK_TYPE_ERRORS("const a = true;", "a const's value must be a constant");
	CHECK_TYPE_ERRORS("function f(a: float) { a++; }", "'++' isn't supported yet");
}

TEST(Typer, ConstsMustBeConstant)
{
	CHECK_TYPE_ERRORS("function f(a: float) { const b = a; }", "a const's value must be a constant");
	CHECK_TYPE_ERRORS("const a = 1; const b = a * 2; const c: [b]float = { 1.0, 2.0 };", "");
}

TEST(Typer, ConstantExpressions)
{
	CHECK_TYPE("const p: [3]float2 = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };",
		"([DECLARATION]CONST:[3]float2 p ([DECLARED_TYPE]ARRAY_TYPE:[3]float2 ([SIZE]CONSTANT:uint 3) "
		"([ELEMENT]REFERENCE:float2 float2 -> TYPE float2)) ([VALUE]CONSTANT:[3]float2 {(0.0, 0.5), (0.5, -0.5), (-0.5, -0.5)}))");
	CHECK_TYPE("const n = 2; const a: [n * 2]int = { 1, 2, 3, n };",
		"([DECLARATION]CONST:int n ([VALUE]CONSTANT:int 2)) ([DECLARATION]CONST:[4]int a ([DECLARED_TYPE]ARRAY_TYPE:[4]int "
		"([SIZE]CONSTANT:int 4) ([ELEMENT]REFERENCE:int int -> TYPE int)) ([VALUE]CONSTANT:[4]int {1, 2, 3, 2}))");
	// Constant indices in code are folded too.
	CHECK_TYPE("const a: [2]float = { 1.0, 2.0 }; function f(): float { return a[1 - 1]; }",
		"([DECLARATION]CONST:[2]float a ([DECLARED_TYPE]ARRAY_TYPE:[2]float ([SIZE]CONSTANT:uint 2) "
		"([ELEMENT]REFERENCE:float float -> TYPE float)) ([VALUE]CONSTANT:[2]float {1.0, 2.0})) "
		"([DECLARATION]FUNCTION f ([RETURN_TYPE]REFERENCE:float float -> TYPE float) ([BODY]BLOCK ([STATEMENT]RETURN "
		"([VALUE]INDEX:float ([OBJECT]REFERENCE:[2]float a -> CONST) ([INDEX]CONSTANT:uint 0)))))");

	uint32 bits[1];
	REQUIRE(ConstantBits(test, "struct S { a: float; b: [2]int; } const s: S = { 1.0, { 2, 3 } }; const x = s.b[1];", bits, 1));
	CHECK_EQ(bits[0], 3u);
	REQUIRE(ConstantBits(test, "struct S { a: float; b: [2]int; } const s: S = { 1.5, { 2, 3 } }; const x = s.a;", bits, 1));
	CHECK_EQ(bits[0], Bits(1.5f));
	CHECK_TYPE_ERRORS("struct S { a: float; } const s: S = { 1.0 }; const x = s.b;", "S has no member 'b'");
	CHECK_TYPE_ERRORS("const v = float4(1.0)[0];", "can't index float4");

	// A const referring to one that failed doesn't get another error.
	CHECK_TYPE_ERRORS("const a = 1.5; const b: int = a; const c = b + 1;", "expected int, got float");

	// Variables and parameters are never constants.
	CHECK_TYPE_ERRORS("function f(v: uint) { const c = v + 1; }", "a const's value must be a constant");
	CHECK_TYPE_ERRORS("function f() { v: uint; x: [v + 1]float; }", "array size must be a constant");

	// Indices in code are checked when they're constant expressions.
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0, 2.0 }; function f(): float { return a[1 + 1]; }", "index 2 is out of bounds for [2]float");
	CHECK_TYPE_ERRORS("const a: [2]float = { 1.0, 2.0 }; function f(i: uint): float { return a[i + 2]; }", "");
}

TEST(Typer, DeclarationsTypedOnFirstUse)
{
	// A is laid out first, which lays out B, which needs n before n's turn.
	CHECK_TYPE_ERRORS("struct A { b: B; } const n: uint = 2; struct B { x: [n]float; }", "");
	CHECK_TYPE_ERRORS("struct A { b: B; } const n: uint = 2; struct B { x: [n]float; } const a: A = { { { 1.0, 2.0 } } };", "");
	// A variable is never a constant, whatever its type.
	CHECK_TYPE_ERRORS("function f() { a: S; v: uint; struct S { x: [v]float; } }", "array size must be a constant");

	CHECK_TYPE_ERRORS("const c: S = { { 1.0 } }; struct S { x: [c]float; }", "'c' depends on itself");
	CHECK_TYPE_ERRORS("function f() { v: S; struct S { x: [v]float; } }", "array size must be a constant");

	// A const's value is resolved before the const is declared, so it can't refer to itself, only to one it shadows.
	CHECK_TYPE_ERRORS("const c: uint = c;", "resolve error: unknown identifier 'c'");
	CHECK_TYPE_ERRORS("const c: uint = 1; function f(): uint { const c: uint = c + 1; return c; }", "");
}

TEST(Typer, ConstantSizeLimits)
{
	// Each four times the size of the one before, for a few bytes of source: a is 32 bytes with its elements, b 64,
	// c 256.
	const char* source =
		"const a: [4]int = { 1, 2, 3, 4 };"
		"const b: [4][4]int = { a, a, a, a };"
		"const c: [4][4][4]int = { b, b, b, b };";

	uint32 previous_size_limit = CONSTANT_SIZE_LIMIT;
	uint64 previous_total_limit = TOTAL_CONSTANT_SIZE_LIMIT;
	DEFER(CONSTANT_SIZE_LIMIT = previous_size_limit);
	DEFER(TOTAL_CONSTANT_SIZE_LIMIT = previous_total_limit);

	CHECK_TYPE_ERRORS(source, "");
	CONSTANT_SIZE_LIMIT = 64;
	CHECK_TYPE_ERRORS(source, "[4][4][4]int is too large for a constant: 256 bytes, at most 64");
	CONSTANT_SIZE_LIMIT = 256;
	TOTAL_CONSTANT_SIZE_LIMIT = 300;
	CHECK_TYPE_ERRORS(source, "constants take more than 300 bytes in total");
}

TEST(Typer, ShaderAttributes)
{
	CHECK_SHADER_TYPE_ERRORS("@vertex function f(@builtin(vertex_index) i: uint): @builtin(position) float4 { return float4(1.0); }", "");
	CHECK_SHADER_TYPE_ERRORS("@fragment function f(): @location(0) float4 { return float4(1.0); }", "");
	CHECK_SHADER_TYPE_ERRORS("struct S { @location(1) a: float4; }", "");
	CHECK_SHADER_TYPE_ERRORS("const SLOT = 2; function f(@location(SLOT) a: float4) {}", "expected uint, got int");
	CHECK_SHADER_TYPE_ERRORS("const SLOT: uint = 2; function f(@location(SLOT) a: float4) {}", "");

	CHECK_SHADER_TYPE_ERRORS("@builtin(position) const a = 1;", "'builtin' can only be used on parameters, fields and return types");
	CHECK_SHADER_TYPE_ERRORS("@location(0) function f() {}", "'location' can only be used on parameters, fields and return types");
	CHECK_SHADER_TYPE_ERRORS("function f(@vertex a: float4) {}", "'vertex' can only be used on functions");
	CHECK_SHADER_TYPE_ERRORS("@vertex(1) function f() {}", "'vertex' takes no arguments");
	CHECK_SHADER_TYPE_ERRORS("function f(@builtin a: uint) {}", "'builtin' takes one argument");
	CHECK_SHADER_TYPE_ERRORS("function f(@location(0, 1) a: float4) {}", "'location' takes one argument");
	CHECK_SHADER_TYPE_ERRORS("function f(@location(1.5) a: float4) {}", "'1.5' is not an integer");
	CHECK_SHADER_TYPE_ERRORS("function f(n: uint, @location(n) a: float4) {}", "a location must be a constant");
	CHECK_SHADER_TYPE_ERRORS("function f(@builtin(0) a: uint) {}", "expected Builtin, got int");
	CHECK_SHADER_TYPE_ERRORS("struct S {} @S function f() {}", "'S' isn't an attribute");
	CHECK_SHADER_TYPE_ERRORS("@1 const a = 1;", "expected an attribute name");
	CHECK_SHADER_TYPE_ERRORS("function f(@(location)(0) a: float4) {}", "");
	CHECK_SHADER_TYPE_ERRORS("function f(@location(0)(1) a: float4) {}", "expected an attribute name");
}

TEST(Typer, TriangleShader)
{
	CHECK_SHADER_TYPE_ERRORS(R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

struct VSOutput
{
	position: float4;
}

@vertex
function VSMain(@builtin(vertex_index) vertex_id: uint): @builtin(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

@fragment
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)",
		"");
}
