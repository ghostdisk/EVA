#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>
#include <string.h>

using namespace EVA;
using namespace EVA::Script;

enum class ParseLevel
{
	EXPRESSION,
	STATEMENT,
	FILE,
};

// Parses source at the given level and serializes the result with SerializeNode. A lone expression or statement is
// marked ROOT; a file gives its declarations, space separated. If parsing fails, gives "error: <message>" instead.
static ZTStringView ParseToString(Arena* arena, ParseLevel level, const char* source)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = arena, .error_arena = arena };
	StringBuilder builder(arena);

	Node* first = nullptr;
	bool parsed = false;
	switch (level)
	{
	case ParseLevel::EXPRESSION:
		first = ParseExpression(parser);
		parsed = first != nullptr;
		break;
	case ParseLevel::STATEMENT:
		first = ParseStatement(parser);
		parsed = first != nullptr;
		break;
	case ParseLevel::FILE:
		parsed = Parse(parser, &first);
		if (parsed)
			first = first->child; // the MODULE's declarations
		break;
	}

	// An expression or statement has to use up the whole source, so a test can't pass by parsing only part of it.
	if (parsed && level != ParseLevel::FILE)
	{
		first->usage = Usage::ROOT;
		parsed = LexToken(parser);
		if (parsed && parser.token.token_type != TokenType::END_OF_FILE)
		{
			EmitError(parser, "unparsed input '%s'", parser.token.start);
			parsed = false;
		}
	}

	if (!parsed)
	{
		builder.Append("error: ");
		for (size_t i = 0; i < parser.errors.size(); ++i)
		{
			if (i)
				builder.Append(" | ");
			builder.Append(parser.errors[i]->message);
		}
		return builder.ToString();
	}

	for (Node* node = first; node; node = node->next)
	{
		if (node != first)
			builder.Append(" ");
		SerializeNode(builder, node);
	}
	return builder.ToString();
}

static void CheckParse(Test::Context& test, const char* file, int line, ParseLevel level, const char* source, StringView expected)
{
	ZTStringView actual = ParseToString(test.arena, level, source);
	if (actual == expected)
		return;
	Test::ReportFailure(test, file, line, "parsing \"%s\"\n    got      %s\n    expected %.*s", source, actual.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_EXPRESSION(source, expected) CheckParse(test, __FILE__, __LINE__, ParseLevel::EXPRESSION, source, expected)
#define CHECK_STATEMENT(source, expected) CheckParse(test, __FILE__, __LINE__, ParseLevel::STATEMENT, source, expected)
#define CHECK_PARSE(source, expected) CheckParse(test, __FILE__, __LINE__, ParseLevel::FILE, source, expected)

// The whole parse must fail with exactly this one error.
#define CHECK_PARSE_ERROR(source, message) CHECK_PARSE(source, "error: " message)

// Binary operators, one entry per precedence level from tightest to loosest binding.
struct OperatorLevel
{
	const char* operators[12];
	bool right_associative;
};

static const OperatorLevel binary_levels[] = {
	{ { "*", "/", "%" } },
	{ { "+", "-" } },
	{ { "<<", ">>" } },
	{ { "<", ">", "<=", ">=" } },
	{ { "==", "!=" } },
	{ { "&" } },
	{ { "^" } },
	{ { "|" } },
	{ { "&&" } },
	{ { "||" } },
	{ { ":" }, true },
	{ { "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=" }, true },
};

static const char* triangle_source = R"(
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
)";

// Expressions

TEST(Parser, Identifier)
{
	CHECK_EXPRESSION("a", "([ROOT]IDENTIFIER a)");
	CHECK_EXPRESSION("  foo_1  ", "([ROOT]IDENTIFIER foo_1)");
}

TEST(Parser, Number)
{
	CHECK_EXPRESSION("1", "([ROOT]NUMBER 1)");
	CHECK_EXPRESSION("1.5", "([ROOT]NUMBER 1.5)");
	CHECK_EXPRESSION(".5", "([ROOT]NUMBER .5)");
	CHECK_EXPRESSION("1e-3", "([ROOT]NUMBER 1e-3)");
}

TEST(Parser, Bool)
{
	CHECK_EXPRESSION("true", "([ROOT]BOOL true)");
	CHECK_EXPRESSION("false", "([ROOT]BOOL false)");
}

TEST(Parser, Prefix)
{
	CHECK_EXPRESSION("-a", "([ROOT]UNARY - ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("+a", "([ROOT]UNARY + ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("!a", "([ROOT]UNARY ! ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("~a", "([ROOT]UNARY ~ ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("++a", "([ROOT]UNARY ++ ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("--a", "([ROOT]UNARY -- ([OPERAND]IDENTIFIER a))");
}

TEST(Parser, PrefixStacks)
{
	CHECK_EXPRESSION("- -a", "([ROOT]UNARY - ([OPERAND]UNARY - ([OPERAND]IDENTIFIER a)))");
	CHECK_EXPRESSION("!-~a", "([ROOT]UNARY ! ([OPERAND]UNARY - ([OPERAND]UNARY ~ ([OPERAND]IDENTIFIER a))))");
}

TEST(Parser, Postfix)
{
	CHECK_EXPRESSION("a++", "([ROOT]POSTFIX ++ ([OPERAND]IDENTIFIER a))");
	CHECK_EXPRESSION("a--", "([ROOT]POSTFIX -- ([OPERAND]IDENTIFIER a))");
}

TEST(Parser, BinaryOperators)
{
	for (const OperatorLevel& level : binary_levels)
	{
		for (const char* const* op = level.operators; *op; ++op)
		{
			ZTStringView source = aprintf(test.arena, "a %s b", *op);
			ZTStringView expected = aprintf(test.arena, "([ROOT]BINARY %s ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b))", *op);
			CHECK_EXPRESSION(source.CString(), expected);
		}
	}
}

TEST(Parser, BinaryPrecedence)
{
	// Every operator against every operator of every looser level, in both orders.
	size_t level_count = sizeof(binary_levels) / sizeof(binary_levels[0]);
	for (size_t tight = 0; tight < level_count; ++tight)
	{
		for (size_t loose = tight + 1; loose < level_count; ++loose)
		{
			for (const char* const* hi = binary_levels[tight].operators; *hi; ++hi)
			{
				for (const char* const* lo = binary_levels[loose].operators; *lo; ++lo)
				{
					CHECK_EXPRESSION(aprintf(test.arena, "a %s b %s c", *hi, *lo).CString(),
						aprintf(test.arena, "([ROOT]BINARY %s ([LEFT]BINARY %s ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)) ([RIGHT]IDENTIFIER c))", *lo, *hi));
					CHECK_EXPRESSION(aprintf(test.arena, "a %s b %s c", *lo, *hi).CString(),
						aprintf(test.arena, "([ROOT]BINARY %s ([LEFT]IDENTIFIER a) ([RIGHT]BINARY %s ([LEFT]IDENTIFIER b) ([RIGHT]IDENTIFIER c)))", *lo, *hi));
				}
			}
		}
	}
}

TEST(Parser, BinaryAssociativity)
{
	for (const OperatorLevel& level : binary_levels)
	{
		for (const char* const* first = level.operators; *first; ++first)
		{
			for (const char* const* second = level.operators; *second; ++second)
			{
				const char* source = aprintf(test.arena, "a %s b %s c", *first, *second).CString();
				if (level.right_associative)
					CHECK_EXPRESSION(source, aprintf(test.arena, "([ROOT]BINARY %s ([LEFT]IDENTIFIER a) ([RIGHT]BINARY %s ([LEFT]IDENTIFIER b) ([RIGHT]IDENTIFIER c)))", *first, *second));
				else
					CHECK_EXPRESSION(source, aprintf(test.arena, "([ROOT]BINARY %s ([LEFT]BINARY %s ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)) ([RIGHT]IDENTIFIER c))", *second, *first));
			}
		}
	}
}

TEST(Parser, PrefixBindsTighterThanBinary)
{
	CHECK_EXPRESSION("-a * b", "([ROOT]BINARY * ([LEFT]UNARY - ([OPERAND]IDENTIFIER a)) ([RIGHT]IDENTIFIER b))");
	CHECK_EXPRESSION("a * -b", "([ROOT]BINARY * ([LEFT]IDENTIFIER a) ([RIGHT]UNARY - ([OPERAND]IDENTIFIER b)))");
	CHECK_EXPRESSION("!a && b", "([ROOT]BINARY && ([LEFT]UNARY ! ([OPERAND]IDENTIFIER a)) ([RIGHT]IDENTIFIER b))");
	CHECK_EXPRESSION("-a = b", "([ROOT]BINARY = ([LEFT]UNARY - ([OPERAND]IDENTIFIER a)) ([RIGHT]IDENTIFIER b))");
	CHECK_EXPRESSION("a - -b", "([ROOT]BINARY - ([LEFT]IDENTIFIER a) ([RIGHT]UNARY - ([OPERAND]IDENTIFIER b)))");
}

TEST(Parser, PostfixBindsTighterThanPrefix)
{
	CHECK_EXPRESSION("-a++", "([ROOT]UNARY - ([OPERAND]POSTFIX ++ ([OPERAND]IDENTIFIER a)))");
	CHECK_EXPRESSION("-a.b", "([ROOT]UNARY - ([OPERAND]MEMBER b ([OBJECT]IDENTIFIER a)))");
	CHECK_EXPRESSION("-f(x)", "([ROOT]UNARY - ([OPERAND]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER x)))");
	CHECK_EXPRESSION("-a[0]", "([ROOT]UNARY - ([OPERAND]INDEX ([OBJECT]IDENTIFIER a) ([INDEX]NUMBER 0)))");
	CHECK_EXPRESSION("a+++b", "([ROOT]BINARY + ([LEFT]POSTFIX ++ ([OPERAND]IDENTIFIER a)) ([RIGHT]IDENTIFIER b))");
	CHECK_EXPRESSION("a * b++", "([ROOT]BINARY * ([LEFT]IDENTIFIER a) ([RIGHT]POSTFIX ++ ([OPERAND]IDENTIFIER b)))");
}

TEST(Parser, Call)
{
	CHECK_EXPRESSION("f()", "([ROOT]CALL ([CALLEE]IDENTIFIER f))");
	CHECK_EXPRESSION("f(a)", "([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER a))");
	CHECK_EXPRESSION("f(a, b)", "([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER a) ([ARGUMENT]IDENTIFIER b))");
	CHECK_EXPRESSION("f(a, b,)", "([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER a) ([ARGUMENT]IDENTIFIER b))");
	CHECK_EXPRESSION("f(a = b)", "([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]BINARY = ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)))");
	CHECK_EXPRESSION("f(g(a), b + c)",
		"([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]CALL ([CALLEE]IDENTIFIER g) ([ARGUMENT]IDENTIFIER a)) "
		"([ARGUMENT]BINARY + ([LEFT]IDENTIFIER b) ([RIGHT]IDENTIFIER c)))");
	CHECK_EXPRESSION("f(@x a)", "([ROOT]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER a ([ATTRIBUTE]IDENTIFIER x)))");
}

TEST(Parser, Member)
{
	CHECK_EXPRESSION("a.b", "([ROOT]MEMBER b ([OBJECT]IDENTIFIER a))");
	CHECK_EXPRESSION("a.b.c", "([ROOT]MEMBER c ([OBJECT]MEMBER b ([OBJECT]IDENTIFIER a)))");
	CHECK_EXPRESSION("(a + b).c", "([ROOT]MEMBER c ([OBJECT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)))");
}

TEST(Parser, Index)
{
	CHECK_EXPRESSION("a[i]", "([ROOT]INDEX ([OBJECT]IDENTIFIER a) ([INDEX]IDENTIFIER i))");
	CHECK_EXPRESSION("a[b][c]", "([ROOT]INDEX ([OBJECT]INDEX ([OBJECT]IDENTIFIER a) ([INDEX]IDENTIFIER b)) ([INDEX]IDENTIFIER c))");
	CHECK_EXPRESSION("a[i + 1]", "([ROOT]INDEX ([OBJECT]IDENTIFIER a) ([INDEX]BINARY + ([LEFT]IDENTIFIER i) ([RIGHT]NUMBER 1)))");
}

TEST(Parser, PostfixChains)
{
	CHECK_EXPRESSION("a.b(c)[d].e",
		"([ROOT]MEMBER e ([OBJECT]INDEX ([OBJECT]CALL ([CALLEE]MEMBER b ([OBJECT]IDENTIFIER a)) ([ARGUMENT]IDENTIFIER c)) "
		"([INDEX]IDENTIFIER d)))");
	CHECK_EXPRESSION("f(a)(b)", "([ROOT]CALL ([CALLEE]CALL ([CALLEE]IDENTIFIER f) ([ARGUMENT]IDENTIFIER a)) ([ARGUMENT]IDENTIFIER b))");
}

TEST(Parser, ArrayType)
{
	CHECK_EXPRESSION("[3]float", "([ROOT]ARRAY_TYPE ([SIZE]NUMBER 3) ([ELEMENT]IDENTIFIER float))");
	CHECK_EXPRESSION("[2][3]float",
		"([ROOT]ARRAY_TYPE ([SIZE]NUMBER 2) ([ELEMENT]ARRAY_TYPE ([SIZE]NUMBER 3) ([ELEMENT]IDENTIFIER float)))");
	CHECK_EXPRESSION("[n + 1]float",
		"([ROOT]ARRAY_TYPE ([SIZE]BINARY + ([LEFT]IDENTIFIER n) ([RIGHT]NUMBER 1)) ([ELEMENT]IDENTIFIER float))");
	CHECK_EXPRESSION("[3]a.b", "([ROOT]ARRAY_TYPE ([SIZE]NUMBER 3) ([ELEMENT]MEMBER b ([OBJECT]IDENTIFIER a)))");
	CHECK_EXPRESSION("x: [3]float = y",
		"([ROOT]BINARY = ([LEFT]BINARY : ([LEFT]IDENTIFIER x) ([RIGHT]ARRAY_TYPE ([SIZE]NUMBER 3) ([ELEMENT]IDENTIFIER float))) "
		"([RIGHT]IDENTIFIER y))");
}

TEST(Parser, InitList)
{
	CHECK_EXPRESSION("{}", "([ROOT]INIT_LIST)");
	CHECK_EXPRESSION("{a, 1}", "([ROOT]INIT_LIST ([ELEMENT]IDENTIFIER a) ([ELEMENT]NUMBER 1))");
	CHECK_EXPRESSION("{a,}", "([ROOT]INIT_LIST ([ELEMENT]IDENTIFIER a))");
	CHECK_EXPRESSION("{{a}, {}}", "([ROOT]INIT_LIST ([ELEMENT]INIT_LIST ([ELEMENT]IDENTIFIER a)) ([ELEMENT]INIT_LIST))");
	CHECK_EXPRESSION("{a + b}", "([ROOT]INIT_LIST ([ELEMENT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)))");
}

TEST(Parser, Parentheses)
{
	CHECK_EXPRESSION("(a)", "([ROOT]IDENTIFIER a)");
	CHECK_EXPRESSION("((a))", "([ROOT]IDENTIFIER a)");
	CHECK_EXPRESSION("(a + b) * c", "([ROOT]BINARY * ([LEFT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)) ([RIGHT]IDENTIFIER c))");
	CHECK_EXPRESSION("a - (b - c)", "([ROOT]BINARY - ([LEFT]IDENTIFIER a) ([RIGHT]BINARY - ([LEFT]IDENTIFIER b) ([RIGHT]IDENTIFIER c)))");
	CHECK_EXPRESSION("(a = b) = c", "([ROOT]BINARY = ([LEFT]BINARY = ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)) ([RIGHT]IDENTIFIER c))");
}

TEST(Parser, WhitespaceAndComments)
{
	CHECK_EXPRESSION("a/*x*/+b", "([ROOT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b))");
	CHECK_EXPRESSION(" a\n+\tb // c", "([ROOT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b))");
}

TEST(Parser, If)
{
	CHECK_EXPRESSION("if a b", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b))");
	CHECK_EXPRESSION("if a b else c", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) ([ELSE]IDENTIFIER c))");
	CHECK_EXPRESSION("if a {} else {}", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]BLOCK))");
	CHECK_EXPRESSION("if a + b c", "([ROOT]IF ([CONDITION]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)) ([THEN]IDENTIFIER c))");
}

TEST(Parser, IfBranchesTakeWholeExpressions)
{
	CHECK_EXPRESSION("if a b + c", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BINARY + ([LEFT]IDENTIFIER b) ([RIGHT]IDENTIFIER c)))");
	CHECK_EXPRESSION("if a b else c + d",
		"([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) ([ELSE]BINARY + ([LEFT]IDENTIFIER c) ([RIGHT]IDENTIFIER d)))");
	CHECK_EXPRESSION("(if a b else c) + d",
		"([ROOT]BINARY + ([LEFT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) ([ELSE]IDENTIFIER c)) ([RIGHT]IDENTIFIER d))");
	CHECK_EXPRESSION("x = if a b else c",
		"([ROOT]BINARY = ([LEFT]IDENTIFIER x) ([RIGHT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) ([ELSE]IDENTIFIER c)))");
}

TEST(Parser, ElseIfChains)
{
	CHECK_EXPRESSION("if a b else if c d else e",
		"([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) "
		"([ELSE]IF ([CONDITION]IDENTIFIER c) ([THEN]IDENTIFIER d) ([ELSE]IDENTIFIER e)))");
	CHECK_EXPRESSION("if a {} else if b {} else {}",
		"([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]IF ([CONDITION]IDENTIFIER b) ([THEN]BLOCK) ([ELSE]BLOCK)))");
}

TEST(Parser, Attributes)
{
	CHECK_EXPRESSION("@a x", "([ROOT]IDENTIFIER x ([ATTRIBUTE]IDENTIFIER a))");
	CHECK_EXPRESSION("@a @b x", "([ROOT]IDENTIFIER x ([ATTRIBUTE]IDENTIFIER a) ([ATTRIBUTE]IDENTIFIER b))");
	CHECK_EXPRESSION("@a(1) x", "([ROOT]IDENTIFIER x ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER a) ([ARGUMENT]NUMBER 1)))");
	CHECK_EXPRESSION("@a x + y", "([ROOT]BINARY + ([ATTRIBUTE]IDENTIFIER a) ([LEFT]IDENTIFIER x) ([RIGHT]IDENTIFIER y))");
}

TEST(Parser, ExpressionMustUseWholeInput)
{
	CHECK_EXPRESSION("a b", "error: unparsed input 'b'");
	CHECK_EXPRESSION("a)", "error: unparsed input ')'");
}

// Statements

TEST(Parser, Return)
{
	CHECK_STATEMENT("return;", "([ROOT]RETURN)");
	CHECK_STATEMENT("return a;", "([ROOT]RETURN ([VALUE]IDENTIFIER a))");
	CHECK_STATEMENT("return a + b;", "([ROOT]RETURN ([VALUE]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)))");
}

TEST(Parser, Block)
{
	CHECK_STATEMENT("{}", "([ROOT]BLOCK)");
	CHECK_STATEMENT("{ a; b; }", "([ROOT]BLOCK ([STATEMENT]IDENTIFIER a) ([STATEMENT]IDENTIFIER b))");
	CHECK_STATEMENT("{ { a; } }", "([ROOT]BLOCK ([STATEMENT]BLOCK ([STATEMENT]IDENTIFIER a)))");
}

TEST(Parser, ExpressionStatement)
{
	CHECK_STATEMENT("a = b;", "([ROOT]BINARY = ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b))");
	CHECK_STATEMENT("f();", "([ROOT]CALL ([CALLEE]IDENTIFIER f))");
	CHECK_STATEMENT("a = b", "error: unexpected end of file, expected ';'");
}

TEST(Parser, IfStatementSemicolons)
{
	// No ';' when the statement ends with a block, one otherwise.
	CHECK_STATEMENT("if a {}", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK))");
	CHECK_STATEMENT("if a b;", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b))");
	CHECK_STATEMENT("if a b", "error: unexpected end of file, expected ';'");
	CHECK_STATEMENT("if a b else {}", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]IDENTIFIER b) ([ELSE]BLOCK))");
	CHECK_STATEMENT("if a {} else b;", "([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]IDENTIFIER b))");
	CHECK_STATEMENT("if a {} else b", "error: unexpected end of file, expected ';'");
	CHECK_STATEMENT("if a {} else if b {}",
		"([ROOT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]IF ([CONDITION]IDENTIFIER b) ([THEN]BLOCK)))");
	CHECK_STATEMENT("if a {} else if b c", "error: unexpected end of file, expected ';'");
}

TEST(Parser, IfStatementInBlock)
{
	CHECK_STATEMENT("{ if a {} b; }",
		"([ROOT]BLOCK ([STATEMENT]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK)) ([STATEMENT]IDENTIFIER b))");
}

TEST(Parser, DeclarationStatement)
{
	CHECK_STATEMENT("const x = 1;", "([ROOT]CONST x ([VALUE]NUMBER 1))");
	CHECK_STATEMENT("{ const x = 1; return x; }",
		"([ROOT]BLOCK ([STATEMENT]CONST x ([VALUE]NUMBER 1)) ([STATEMENT]RETURN ([VALUE]IDENTIFIER x)))");
}

// Declarations

TEST(Parser, EmptyFile)
{
	CHECK_PARSE("", "");
	CHECK_PARSE(" // nothing\n", "");
}

TEST(Parser, Module)
{
	// CHECK_PARSE shows only the declarations, this checks the MODULE node holding them.
	const char* source = "const a = 1; function f() {}";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	Node* module = nullptr;
	REQUIRE(Parse(parser, &module));

	StringBuilder builder(test.arena);
	SerializeNode(builder, module);
	CHECK_EQ(builder.ToString(), "([ROOT]MODULE ([DECLARATION]CONST a ([VALUE]NUMBER 1)) ([DECLARATION]FUNCTION f ([BODY]BLOCK)))");
}

TEST(Parser, Const)
{
	CHECK_PARSE("const x = 1;", "([DECLARATION]CONST x ([VALUE]NUMBER 1))");
	CHECK_PARSE("const x: int = 1;", "([DECLARATION]CONST x ([TYPE]IDENTIFIER int) ([VALUE]NUMBER 1))");
	CHECK_PARSE("const x: [2]int = {1, 2};",
		"([DECLARATION]CONST x ([TYPE]ARRAY_TYPE ([SIZE]NUMBER 2) ([ELEMENT]IDENTIFIER int)) "
		"([VALUE]INIT_LIST ([ELEMENT]NUMBER 1) ([ELEMENT]NUMBER 2)))");
}

TEST(Parser, ConstSemicolon)
{
	// Required unless the value ends with a block, where it's optional. An init list isn't a block.
	CHECK_PARSE_ERROR("const x = 1", "unexpected end of file, expected ';'");
	CHECK_PARSE_ERROR("const x = {1, 2}", "unexpected end of file, expected ';'");
	CHECK_PARSE_ERROR("const x = if a {} else b", "unexpected end of file, expected ';'");
	CHECK_PARSE_ERROR("const x = 1 const y = 2;", "unexpected token 'const', expected ';'");

	CHECK_PARSE("const x = if a {} else {}", "([DECLARATION]CONST x ([VALUE]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]BLOCK)))");
	CHECK_PARSE("const x = if a {} else {};", "([DECLARATION]CONST x ([VALUE]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]BLOCK)))");
	CHECK_PARSE("const x = if a {} else b;", "([DECLARATION]CONST x ([VALUE]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]IDENTIFIER b)))");
	CHECK_PARSE("const x = if a {} else {} const y = 1;",
		"([DECLARATION]CONST x ([VALUE]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]BLOCK))) ([DECLARATION]CONST y ([VALUE]NUMBER 1))");

	CHECK_STATEMENT("{ const x = if a {} else {} return x; }",
		"([ROOT]BLOCK ([STATEMENT]CONST x ([VALUE]IF ([CONDITION]IDENTIFIER a) ([THEN]BLOCK) ([ELSE]BLOCK))) "
		"([STATEMENT]RETURN ([VALUE]IDENTIFIER x)))");
	CHECK_STATEMENT("{ const x = 1 return x; }", "error: unexpected token 'return', expected ';'");
}

TEST(Parser, Struct)
{
	CHECK_PARSE("struct S {}", "([DECLARATION]STRUCT S)");
	CHECK_PARSE("struct S { a: int; b: float = 1; }",
		"([DECLARATION]STRUCT S ([MEMBER]FIELD a ([TYPE]IDENTIFIER int)) ([MEMBER]FIELD b ([TYPE]IDENTIFIER float) ([VALUE]NUMBER 1)))");
	CHECK_PARSE("struct S { @x a: int; }", "([DECLARATION]STRUCT S ([MEMBER]FIELD a ([ATTRIBUTE]IDENTIFIER x) ([TYPE]IDENTIFIER int)))");
}

TEST(Parser, Function)
{
	CHECK_PARSE("function f() {}", "([DECLARATION]FUNCTION f ([BODY]BLOCK))");
	CHECK_PARSE("function f(): int {}", "([DECLARATION]FUNCTION f ([RETURN_TYPE]IDENTIFIER int) ([BODY]BLOCK))");
	CHECK_PARSE("function f() { a = 1; return a; }",
		"([DECLARATION]FUNCTION f ([BODY]BLOCK ([STATEMENT]BINARY = ([LEFT]IDENTIFIER a) ([RIGHT]NUMBER 1)) "
		"([STATEMENT]RETURN ([VALUE]IDENTIFIER a))))");
}

TEST(Parser, FunctionParameters)
{
	CHECK_PARSE("function f(a: int) {}", "([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]IDENTIFIER int)) ([BODY]BLOCK))");
	CHECK_PARSE("function f(a: int,) {}", "([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]IDENTIFIER int)) ([BODY]BLOCK))");
	CHECK_PARSE("function f(a: int, b: float = 1) {}",
		"([DECLARATION]FUNCTION f ([PARAMETER]PARAMETER a ([TYPE]IDENTIFIER int)) "
		"([PARAMETER]PARAMETER b ([TYPE]IDENTIFIER float) ([VALUE]NUMBER 1)) ([BODY]BLOCK))");
}

TEST(Parser, FunctionAttributes)
{
	CHECK_PARSE("function f(@builtin(vertex_index) id: uint): @builtin(position) float4 { return id; }",
		"([DECLARATION]FUNCTION f "
		"([PARAMETER]PARAMETER id ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER builtin) ([ARGUMENT]IDENTIFIER vertex_index)) ([TYPE]IDENTIFIER uint)) "
		"([RETURN_TYPE]IDENTIFIER float4 ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER builtin) ([ARGUMENT]IDENTIFIER position))) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]IDENTIFIER id))))");
}

TEST(Parser, DeclarationAttributes)
{
	CHECK_PARSE("@a function f() {}", "([DECLARATION]FUNCTION f ([ATTRIBUTE]IDENTIFIER a) ([BODY]BLOCK))");
	CHECK_PARSE("@a @b struct S {}", "([DECLARATION]STRUCT S ([ATTRIBUTE]IDENTIFIER a) ([ATTRIBUTE]IDENTIFIER b))");
	CHECK_PARSE("@a const x = 1;", "([DECLARATION]CONST x ([ATTRIBUTE]IDENTIFIER a) ([VALUE]NUMBER 1))");
}

TEST(Parser, MultipleDeclarations)
{
	CHECK_PARSE("const a = 1; struct S {} function f() {}",
		"([DECLARATION]CONST a ([VALUE]NUMBER 1)) ([DECLARATION]STRUCT S) ([DECLARATION]FUNCTION f ([BODY]BLOCK))");
}

TEST(Parser, TriangleShader)
{
	CHECK_PARSE(triangle_source,
		"([DECLARATION]CONST positions ([TYPE]ARRAY_TYPE ([SIZE]NUMBER 3) ([ELEMENT]IDENTIFIER float2)) ([VALUE]INIT_LIST "
		"([ELEMENT]CALL ([CALLEE]IDENTIFIER float2) ([ARGUMENT]NUMBER 0.0) ([ARGUMENT]NUMBER 0.5)) "
		"([ELEMENT]CALL ([CALLEE]IDENTIFIER float2) ([ARGUMENT]NUMBER 0.5) ([ARGUMENT]UNARY - ([OPERAND]NUMBER 0.5))) "
		"([ELEMENT]CALL ([CALLEE]IDENTIFIER float2) ([ARGUMENT]UNARY - ([OPERAND]NUMBER 0.5)) ([ARGUMENT]UNARY - ([OPERAND]NUMBER 0.5))))) "
		"([DECLARATION]STRUCT VSOutput ([MEMBER]FIELD position ([TYPE]IDENTIFIER float4))) "
		"([DECLARATION]FUNCTION VSMain "
		"([PARAMETER]PARAMETER vertex_id ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER builtin) ([ARGUMENT]IDENTIFIER vertex_index)) ([TYPE]IDENTIFIER uint)) "
		"([RETURN_TYPE]IDENTIFIER float4 ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER builtin) ([ARGUMENT]IDENTIFIER position))) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]CALL ([CALLEE]IDENTIFIER float4) "
		"([ARGUMENT]INDEX ([OBJECT]IDENTIFIER positions) ([INDEX]IDENTIFIER vertex_id)) ([ARGUMENT]NUMBER 0.0) ([ARGUMENT]NUMBER 1.0))))) "
		"([DECLARATION]FUNCTION PSMain "
		"([RETURN_TYPE]IDENTIFIER float4 ([ATTRIBUTE]CALL ([CALLEE]IDENTIFIER location) ([ARGUMENT]NUMBER 0))) "
		"([BODY]BLOCK ([STATEMENT]RETURN ([VALUE]CALL ([CALLEE]IDENTIFIER float4) "
		"([ARGUMENT]NUMBER 1.0) ([ARGUMENT]NUMBER 1.0) ([ARGUMENT]NUMBER 1.0) ([ARGUMENT]NUMBER 1.0)))))");
}

// Errors

TEST(Parser, FunctionErrors)
{
	CHECK_PARSE_ERROR("function ", "unexpected end of file");
	CHECK_PARSE_ERROR("function 1() {}", "unexpected token '1'");
	CHECK_PARSE_ERROR("function f", "unexpected end of file, expected '('");
	CHECK_PARSE_ERROR("function f(", "unexpected end of file");
	CHECK_PARSE_ERROR("function f()", "unexpected end of file, expected '{'");
	CHECK_PARSE_ERROR("function f() {", "unexpected end of file");
	CHECK_PARSE_ERROR("function f(a) {}", "'a' needs a type");
	CHECK_PARSE_ERROR("function f(a: int b: int) {}", "unexpected token 'b', expected ')'");
	CHECK_PARSE_ERROR("function f() { a = b }", "unexpected token '}', expected ';'");
}

TEST(Parser, StructErrors)
{
	CHECK_PARSE_ERROR("struct", "unexpected end of file");
	CHECK_PARSE_ERROR("struct { }", "unexpected token '{'");
	CHECK_PARSE_ERROR("struct S", "unexpected end of file, expected '{'");
	CHECK_PARSE_ERROR("struct S { a; }", "'a' needs a type");
	CHECK_PARSE_ERROR("struct S { a: int }", "unexpected token '}', expected ';'");
}

TEST(Parser, ConstErrors)
{
	CHECK_PARSE_ERROR("const", "unexpected end of file");
	CHECK_PARSE_ERROR("const x", "'x' needs a value");
	CHECK_PARSE_ERROR("const x: int", "'x' needs a value");
	CHECK_PARSE_ERROR("const 3 = 1", "expected name [: type] [= value]");
	CHECK_PARSE_ERROR("const x += 1", "expected name [: type] [= value]");
	CHECK_PARSE_ERROR("const a.b = 1", "expected name [: type] [= value]");
}

TEST(Parser, TopLevelErrors)
{
	CHECK_PARSE_ERROR("x", "unexpected token 'x'");
	CHECK_PARSE_ERROR("1", "unexpected token '1'");
	CHECK_PARSE_ERROR("@a", "unexpected end of file");
	CHECK_PARSE_ERROR("@a x", "unexpected token 'x'");
	CHECK_PARSE_ERROR("const x = 1 }", "unexpected token '}', expected ';'");
}

TEST(Parser, ExpressionErrors)
{
	CHECK_PARSE_ERROR("const x = (1", "unexpected end of file, expected ')'");
	CHECK_PARSE_ERROR("const x = f(1", "unexpected end of file, expected ')'");
	CHECK_PARSE_ERROR("const x = f(1 2)", "unexpected token '2', expected ')'");
	CHECK_PARSE_ERROR("const x = a[1", "unexpected end of file, expected ']'");
	CHECK_PARSE_ERROR("const x = {1, 2", "unexpected end of file, expected '}'");
	CHECK_PARSE_ERROR("const x = [3", "unexpected end of file, expected ']'");
	CHECK_PARSE_ERROR("const x = a.", "unexpected end of file");
	CHECK_PARSE_ERROR("const x = a.1", "unexpected token '.1', expected ';'");
	CHECK_PARSE_ERROR("const x = 1 +", "unexpected end of file");
	CHECK_PARSE_ERROR("const x = * 1", "unexpected token '*'");
	CHECK_PARSE_ERROR("const x = ()", "unexpected token ')'");
	CHECK_PARSE_ERROR("const x = if", "unexpected end of file");
	CHECK_PARSE_ERROR("const x = if a", "unexpected end of file");
	CHECK_PARSE_ERROR("const x = if a b else", "unexpected end of file");
}

TEST(Parser, LexerErrors)
{
	CHECK_PARSE_ERROR("/* x", "unterminated block comment");
	CHECK_PARSE_ERROR("#", "unexpected character '#'");
	CHECK_PARSE_ERROR("const x = 1 #", "unexpected character '#'");
	CHECK_PARSE_ERROR("function f() { a = $; }", "unexpected character '$'");
}

// Limits

static ZTStringView Repeat(Arena* arena, const char* prefix, const char* repeated, uint32 count, const char* middle, const char* closing)
{
	StringBuilder builder(arena);
	builder.Append(prefix);
	for (uint32 i = 0; i < count; ++i)
		builder.Append(repeated);
	builder.Append(middle);
	for (uint32 i = 0; i < count; ++i)
		builder.Append(closing);
	return builder.ToString();
}

// Lowers RECURSION_LIMIT until the end of the scope, so sources that hit it stay short.
#define SET_RECURSION_LIMIT(limit)                        \
	uint32 previous_recursion_limit = RECURSION_LIMIT;    \
	RECURSION_LIMIT = limit;                              \
	DEFER(RECURSION_LIMIT = previous_recursion_limit)

static void CheckParses(Test::Context& test, const char* file, int line, ParseLevel level, const char* source)
{
	ZTStringView actual = ParseToString(test.arena, level, source);
	if (StringView(actual.CString(), actual.length < 7 ? actual.length : 7) == "error: ")
		Test::ReportFailure(test, file, line, "parsing \"%s\"\n    failed with %s", source, actual.CString());
}

#define CHECK_EXPRESSION_PARSES(source) CheckParses(test, __FILE__, __LINE__, ParseLevel::EXPRESSION, source)
#define CHECK_STATEMENT_PARSES(source) CheckParses(test, __FILE__, __LINE__, ParseLevel::STATEMENT, source)
#define CHECK_FILE_PARSES(source) CheckParses(test, __FILE__, __LINE__, ParseLevel::FILE, source)

TEST(ParserRecursion, EachRecursiveConstruct)
{
	// Right at a limit of 4 and one level past it. Every expression, statement, if, function and struct being parsed
	// counts one level; const itself doesn't.
	SET_RECURSION_LIMIT(4);

	CHECK_EXPRESSION_PARSES("(((a)))");
	CHECK_EXPRESSION("((((a))))", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("f(f(f(a)))");
	CHECK_EXPRESSION("f(f(f(f(a))))", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("a[a[a[i]]]");
	CHECK_EXPRESSION("a[a[a[a[i]]]]", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("[[[1]x]x]x");
	CHECK_EXPRESSION("[[[[1]x]x]x]x", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("{{{a}}}");
	CHECK_EXPRESSION("{{{{a}}}}", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("@@@a b c d"); // an attribute's expression can start with attributes too
	CHECK_EXPRESSION("@@@@a b c d e", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("if a if a b");
	CHECK_EXPRESSION("if a if a if a b", "error: nested too deeply");
	CHECK_EXPRESSION_PARSES("if a b else if a b else c");
	CHECK_EXPRESSION("if a b else if a b else if a b else c", "error: nested too deeply");

	CHECK_STATEMENT_PARSES("{{{{}}}}");
	CHECK_STATEMENT("{{{{{}}}}}", "error: nested too deeply");
	CHECK_STATEMENT_PARSES("if a { b; }");
	CHECK_STATEMENT("if a { if a {} }", "error: nested too deeply");
	CHECK_STATEMENT_PARSES("return ((a));");
	CHECK_STATEMENT("return (((a)));", "error: nested too deeply");

	CHECK_FILE_PARSES("function f() { function g() {} }");
	CHECK_PARSE_ERROR("function f() { function g() { function h() {} } }", "nested too deeply");
	CHECK_FILE_PARSES("function f(a: ((b))): ((c)) {}");
	CHECK_PARSE_ERROR("function f(a: (((b)))) {}", "nested too deeply");
	CHECK_PARSE_ERROR("function f(): (((c))) {}", "nested too deeply");
	CHECK_FILE_PARSES("struct S { a: ((b)); }");
	CHECK_PARSE_ERROR("struct S { a: (((b))); }", "nested too deeply");
	CHECK_FILE_PARSES("const x = (((a)));");
	CHECK_PARSE_ERROR("const x = ((((a))));", "nested too deeply");
}

TEST(ParserRecursion, MixedConstructsShareOneCounter)
{
	// function, if statement, block, return, parentheses, if expression, array size, call: 11 levels at the deepest.
	const char* source = "function f() { if a { { return (if b [f(c)]d else { e; }); } } }";
	{
		SET_RECURSION_LIMIT(11);
		CHECK_FILE_PARSES(source);
	}
	{
		SET_RECURSION_LIMIT(10);
		CHECK_PARSE_ERROR(source, "nested too deeply");
	}
}

TEST(ParserRecursion, DepthIsRestored)
{
	SET_RECURSION_LIMIT(4);
	const char* sources[] = { "function f() { if a { b; } }", "function f() { function g() { function h() {} } }" };
	for (const char* source : sources)
	{
		Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
		Node* module = nullptr;
		Parse(parser, &module);
		CHECK_EQ(parser.recursion_depth, 0u);
	}
}

TEST(ParserRecursion, DefaultLimitStopsDeepNesting)
{
	CHECK_EXPRESSION(Repeat(test.arena, "", "(", 10000, "a", ")").CString(), "error: nested too deeply");
	CHECK_STATEMENT(Repeat(test.arena, "", "{", 10000, "", "}").CString(), "error: nested too deeply");
}

TEST(Parser, LongPrefixChainDoesNotRecurse)
{
	// Prefix operators wait on the parser's operator stack rather than recursing, so they aren't limited by nesting.
	ZTStringView source = Repeat(test.arena, "", "!", 10000, "a", "");
	Parser parser = { .source = (char*)source.CString(), .head = (char*)source.CString(), .arena = test.arena, .error_arena = test.arena };
	Node* node = ParseExpression(parser);
	REQUIRE(node);
	CHECK_EQ(node->type, NodeType::UNARY);
}

// Robustness

// Parses source, which must either succeed or report an error.
static void CheckParseTerminates(Test::Context& test, const char* source)
{
	uint8* mark = test.arena->head;
	{
		Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
		Node* module = nullptr;
		if (!Parse(parser, &module) && parser.errors.empty())
			Test::ReportFailure(test, __FILE__, __LINE__, "parse failed without an error for:\n%s", source);
	}
	test.arena->head = mark;
}

TEST(Parser, EveryPrefixOfAShaderTerminates)
{
	size_t length = strlen(triangle_source);
	char* buffer = (char*)test.arena->Allocate(length + 1);
	for (size_t prefix = 0; prefix <= length; ++prefix)
	{
		memcpy(buffer, triangle_source, prefix);
		buffer[prefix] = '\0';
		CheckParseTerminates(test, buffer);
	}
}

TEST(Parser, EveryOneByteDeletionOfAShaderTerminates)
{
	size_t length = strlen(triangle_source);
	char* buffer = (char*)test.arena->Allocate(length + 1);
	for (size_t deleted = 0; deleted < length; ++deleted)
	{
		memcpy(buffer, triangle_source, deleted);
		memcpy(buffer + deleted, triangle_source + deleted + 1, length - deleted - 1);
		buffer[length - 1] = '\0';
		CheckParseTerminates(test, buffer);
	}
}
