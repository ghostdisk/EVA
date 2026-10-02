#include <EVA/Test/Test.hpp>
#include <EVA/Script/Script.hpp>
#include <math.h>

using namespace EVA;
using namespace EVA::Script;

// Lexes source to the end. Each token is written as TokenToString, plus its text for identifiers and numbers, space
// separated: "a <<= 1" gives "identifier(a) <<= number(1)". A lex error ends the string with "error: <message>".
static ZTStringView LexToString(Arena* arena, const char* source)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = arena, .error_arena = arena };
	StringBuilder builder(arena);
	for (;;)
	{
		if (!LexToken(parser))
		{
			builder.Append(builder.length ? " error: " : "error: ");
			builder.Append(parser.errors.back()->message);
			break;
		}

		Token token = parser.token;
		if (token.token_type == TokenType::END_OF_FILE)
			break;

		if (builder.length)
			builder.Append(" ");
		builder.Append(TokenToString(token.token_type));
		if (token.token_type == TokenType::IDENTIFIER || token.token_type == TokenType::NUMBER)
			builder.AppendFormat("(%.*s)", (int)(token.end - token.start), token.start);
		EatToken(parser);
	}
	return builder.ToString();
}

static void CheckTokens(Test::Context& test, const char* file, int line, const char* source, StringView expected)
{
	ZTStringView actual = LexToString(test.arena, source);
	if (actual == expected)
		return;
	Test::ReportFailure(test, file, line, "lexing \"%s\"\n    got      %s\n    expected %.*s", source, actual.CString(),
		(int)expected.length, (const char*)expected.data);
}

#define CHECK_TOKENS(source, expected) CheckTokens(test, __FILE__, __LINE__, source, expected)

TEST(Lexer, Empty)
{
	CHECK_TOKENS("", "");
	CHECK_TOKENS(" \t\r\n ", "");
	CHECK_TOKENS("// comment", "");
	CHECK_TOKENS("/* comment */", "");
}

TEST(Lexer, Identifiers)
{
	CHECK_TOKENS("a _b c1 CamelCase __x9", "identifier(a) identifier(_b) identifier(c1) identifier(CamelCase) identifier(__x9)");
}

TEST(Lexer, Keywords)
{
	CHECK_TOKENS("const struct function if else return true false", "const struct function if else return true false");
}

TEST(Lexer, KeywordLookalikesAreIdentifiers)
{
	CHECK_TOKENS("iffy functions functio f returned If ELSE truex _if",
		"identifier(iffy) identifier(functions) identifier(functio) identifier(f) identifier(returned) identifier(If) "
		"identifier(ELSE) identifier(truex) identifier(_if)");
}

TEST(Lexer, IdentifiersGetAtoms)
{
	const char* source = "foo if";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };

	REQUIRE(LexToken(parser));
	CHECK_EQ(parser.token.atom, GetAtom("foo"));
	EatToken(parser);

	REQUIRE(LexToken(parser));
	CHECK_EQ(parser.token.token_type, TokenType::KW_IF);
	CHECK_EQ(parser.token.atom, Atom::NONE);
}

TEST(Lexer, Numbers)
{
	CHECK_TOKENS("0 123 1.5 .5 1e3 1e-3 1E+3",
		"number(0) number(123) number(1.5) number(.5) number(1e3) number(1e-3) number(1E+3)");
}

// Lexes source as a single NUMBER token.
static NumberLiteral* LexNumber(Test::Context& test, const char* source)
{
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	if (!LexToken(parser) || parser.token.token_type != TokenType::NUMBER)
		return nullptr;
	return parser.token.number;
}

TEST(Lexer, NumberValues)
{
	NumberLiteral* number = LexNumber(test, "123");
	REQUIRE(number);
	CHECK_EQ(number->kind, NumberKind::INTEGER);
	CHECK_EQ(number->integer, (uint64)123);

	number = LexNumber(test, "0x1F");
	REQUIRE(number);
	CHECK_EQ(number->kind, NumberKind::INTEGER);
	CHECK_EQ(number->integer, (uint64)31);

	number = LexNumber(test, "18446744073709551615");
	REQUIRE(number);
	CHECK_EQ(number->integer, UINT64_MAX);

	// Both precisions are parsed from the text, so neither is rounded twice.
	number = LexNumber(test, "0.1");
	REQUIRE(number);
	CHECK_EQ(number->kind, NumberKind::FLOAT);
	CHECK_EQ(number->f64, 0.1);
	CHECK_EQ(number->f32, 0.1f);

	number = LexNumber(test, "1e3");
	REQUIRE(number);
	CHECK_EQ(number->kind, NumberKind::FLOAT);
	CHECK_EQ(number->f64, 1000.0);

	// Too large for a float but not a double: the typer decides whether that's an error.
	number = LexNumber(test, "1e40");
	REQUIRE(number);
	CHECK_EQ(number->f64, 1e40);
	CHECK(isinf(number->f32));
}

TEST(Lexer, InvalidNumbers)
{
	CHECK_TOKENS("1x", "error: invalid number '1x'");
	CHECK_TOKENS("1.5f", "error: invalid number '1.5f'");
	CHECK_TOKENS("0x", "error: invalid number '0x'");
	CHECK_TOKENS("0xG", "error: invalid number '0xG'");
	CHECK_TOKENS("1e", "error: invalid number '1e'");
	CHECK_TOKENS("1.2.3", "error: invalid number '1.2.3'");
	CHECK_TOKENS("18446744073709551616", "error: '18446744073709551616' is out of range");
	CHECK_TOKENS("0x10000000000000000", "error: '0x10000000000000000' is out of range");
	CHECK_TOKENS("1e400", "error: '1e400' is out of range");
}

TEST(Lexer, DotBeforeDigitStartsANumber)
{
	CHECK_TOKENS(".5", "number(.5)");
	CHECK_TOKENS("a.b", "identifier(a) . identifier(b)");
	CHECK_TOKENS("a . 5", "identifier(a) . number(5)");
}

TEST(Lexer, SingleCharOperators)
{
	CHECK_TOKENS("; , + - = * / % & | ^ ~ ! : . < > ( ) [ ] { } @", "; , + - = * / % & | ^ ~ ! : . < > ( ) [ ] { } @");
}

TEST(Lexer, MultiCharOperators)
{
	CHECK_TOKENS("+= -= *= /= %= &= |= ^= <<= >>= ++ -- << >> == != <= >= && ||",
		"+= -= *= /= %= &= |= ^= <<= >>= ++ -- << >> == != <= >= && ||");
}

TEST(Lexer, LongestOperatorWins)
{
	CHECK_TOKENS("a<<=b", "identifier(a) <<= identifier(b)");
	CHECK_TOKENS("<<<", "<< <");
	CHECK_TOKENS("a+++b", "identifier(a) ++ + identifier(b)");
	CHECK_TOKENS("===", "== =");
	CHECK_TOKENS("!==", "!= =");
	CHECK_TOKENS("&&&", "&& &");
	CHECK_TOKENS("->", "- >");
	CHECK_TOKENS(">>=>", ">>= >");
	CHECK_TOKENS("< <", "< <");
}

TEST(Lexer, NoSpacesNeeded)
{
	CHECK_TOKENS("a+b*c", "identifier(a) + identifier(b) * identifier(c)");
	CHECK_TOKENS("f(x)[0].y", "identifier(f) ( identifier(x) ) [ number(0) ] . identifier(y)");
	CHECK_TOKENS("@a(1)b", "@ identifier(a) ( number(1) ) identifier(b)");
}

TEST(Lexer, LineComments)
{
	CHECK_TOKENS("a // comment\nb", "identifier(a) identifier(b)");
	CHECK_TOKENS("a//b\nc", "identifier(a) identifier(c)");
	CHECK_TOKENS("a //\r\nb", "identifier(a) identifier(b)");
	CHECK_TOKENS("a // comment at the end", "identifier(a)");
	CHECK_TOKENS("// /* \na", "identifier(a)");
}

TEST(Lexer, BlockComments)
{
	CHECK_TOKENS("a/**/b", "identifier(a) identifier(b)");
	CHECK_TOKENS("a /* x\ny */ b", "identifier(a) identifier(b)");
	CHECK_TOKENS("a/***/b", "identifier(a) identifier(b)");
	CHECK_TOKENS("/* * / */a", "identifier(a)");
	CHECK_TOKENS("/* // */ a", "identifier(a)");
	CHECK_TOKENS("/* a */ /* b */ c", "identifier(c)");
}

TEST(Lexer, SlashIsNotAComment)
{
	CHECK_TOKENS("a / b", "identifier(a) / identifier(b)");
	CHECK_TOKENS("a /= b", "identifier(a) /= identifier(b)");
	CHECK_TOKENS("a/b", "identifier(a) / identifier(b)");
}

TEST(Lexer, UnterminatedBlockComment)
{
	CHECK_TOKENS("/*", "error: unterminated block comment");
	CHECK_TOKENS("a /* b", "identifier(a) error: unterminated block comment");
	CHECK_TOKENS("/* *", "error: unterminated block comment");
	CHECK_TOKENS("/* */ /*", "error: unterminated block comment");
}

TEST(Lexer, UnexpectedCharacter)
{
	CHECK_TOKENS("#", "error: unexpected character '#'");
	CHECK_TOKENS("a $", "identifier(a) error: unexpected character '$'");
	CHECK_TOKENS("\"", "error: unexpected character '\"'");
	CHECK_TOKENS("?", "error: unexpected character '?'");
	CHECK_TOKENS("\\", "error: unexpected character '\\'");
	CHECK_TOKENS("\x80", "error: unexpected byte 0x80");
	CHECK_TOKENS("\x01", "error: unexpected byte 0x01");
	CHECK_TOKENS("\x7F", "error: unexpected byte 0x7F");
}

TEST(Lexer, TokenIsLexedOnceUntilEaten)
{
	const char* source = "foo bar";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };

	REQUIRE(LexToken(parser));
	Token first = parser.token;
	char* head = parser.head;
	REQUIRE(LexToken(parser));
	CHECK(parser.token.start == first.start);
	CHECK(parser.head == head);

	EatToken(parser);
	REQUIRE(LexToken(parser));
	CHECK_EQ(StringView(parser.token.start, parser.token.end - parser.token.start), "bar");
}

TEST(Lexer, TokenTextPointsIntoSource)
{
	const char* source = "  foo";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	REQUIRE(LexToken(parser));
	CHECK(parser.token.start == source + 2);
	CHECK(parser.token.end == source + 5);
}

TEST(Lexer, EndOfFileRepeats)
{
	const char* source = "a";
	Parser parser = { .source = (char*)source, .head = (char*)source, .arena = test.arena, .error_arena = test.arena };
	REQUIRE(LexToken(parser));
	EatToken(parser);
	for (int i = 0; i < 3; ++i)
	{
		REQUIRE(LexToken(parser));
		CHECK_EQ(parser.token.token_type, TokenType::END_OF_FILE);
		EatToken(parser);
	}
}
