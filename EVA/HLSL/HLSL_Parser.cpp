#include <EVA/HLSL/HLSL.hpp>
#include <stdarg.h>

namespace EVA::HLSL
{

// Returns false / nullptr from the calling function if expr is falsy.
#define TRY(expr) do { if (!(expr)) return {}; } while (0)

static bool Error(Parser& parser, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(parser.lexer.error_buffer, sizeof(parser.lexer.error_buffer), format, args);
	va_end(args);
	return false;
}

static bool UnexpectedToken(Parser& parser)
{
	Token& token = parser.lexer.token;
	if (token.token_type == TokenType::END_OF_FILE)
		return Error(parser, "unexpected end of file");
	return Error(parser, "unexpected token '%.*s'", (int)(token.end - token.start), token.start);
}

// Returns the current token, lexing it if needed. nullptr on lex error.
static Token* Peek(Parser& parser)
{
	if (!LexToken(parser.lexer))
		return nullptr;
	return &parser.lexer.token;
}

static bool ParseAttributes(Parser& parser, Node** out_attributes)
{
	return Error(parser, "%s not implemented", __func__);
}

static bool ParseModifiers(Parser& parser, uint32* out_modifiers)
{
	return Error(parser, "%s not implemented", __func__);
}

static bool ParseType(Parser& parser, Node** out_type)
{
	return Error(parser, "%s not implemented", __func__);
}

static bool ParseStruct(Parser& parser)
{
	return Error(parser, "%s not implemented", __func__);
}

static bool ParseTypedef(Parser& parser)
{
	return Error(parser, "%s not implemented", __func__);
}

// Called with the name eaten and '(' as the current token.
static bool ParseFunction(Parser& parser, Node* attributes, uint32 modifiers, Node* return_type, Token name)
{
	return Error(parser, "%s not implemented", __func__);
}

// Called with the first name eaten. Parses the rest of the declarator list up to and including ';'.
static bool ParseVariables(Parser& parser, uint32 modifiers, Node* type, Token name)
{
	return Error(parser, "%s not implemented", __func__);
}

// Parses one top-level declaration, appending its node(s) via parser.tail.
static bool ParseTopLevel(Parser& parser)
{
	Token* token = Peek(parser);
	TRY(token);

	if (token->token_type == TokenType::SEMICOLON)
	{
		EatToken(parser.lexer);
		return true;
	}

	if (token->token_type == TokenType::KW_STRUCT)
		return ParseStruct(parser);

	if (token->token_type == TokenType::KW_TYPEDEF)
		return ParseTypedef(parser);

	// modifiers* type name, followed by either '(' for a function or the rest of a variable declarator list.
	Node* attributes = nullptr;
	if (token->token_type == TokenType::LEFT_BRACKET)
		TRY(ParseAttributes(parser, &attributes));

	uint32 modifiers = 0;
	TRY(ParseModifiers(parser, &modifiers));

	Node* type = nullptr;
	TRY(ParseType(parser, &type));

	TRY(token = Peek(parser));
	if (token->token_type != TokenType::IDENTIFIER)
		return UnexpectedToken(parser);
	Token name = *token;
	EatToken(parser.lexer);

	TRY(token = Peek(parser));
	if (token->token_type == TokenType::LEFT_PAREN)
		return ParseFunction(parser, attributes, modifiers, type, name);

	if (attributes)
		return Error(parser, "attributes are only allowed on functions");
	return ParseVariables(parser, modifiers, type, name);
}

bool Parse(Parser& parser, Node** out_declarations)
{
	parser.tail = out_declarations;
	for (;;)
	{
		Token* token = Peek(parser);
		TRY(token);
		if (token->token_type == TokenType::END_OF_FILE)
			return true;
		TRY(ParseTopLevel(parser));
	}
}

}
