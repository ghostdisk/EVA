#include <EVA/HLSL/HLSL.hpp>
#include <stdarg.h>

namespace EVA::HLSL
{

// Returns false / nullptr from the calling function if expr is falsy.
#define TRY(expr)      \
	do                 \
	{                  \
		if (!(expr))   \
			return {}; \
	} while (0)

static bool Error(Parser& parser, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(parser.error_buffer, sizeof(parser.error_buffer), format, args);
	va_end(args);
	return false;
}

static bool UnexpectedToken(Parser& parser)
{
	Token& token = parser.token;
	if (token.token_type == TokenType::END_OF_FILE)
		return Error(parser, "unexpected end of file");
	return Error(parser, "unexpected token '%.*s'", (int)(token.end - token.start), token.start);
}

static bool ParseAttributes(Parser& parser, Node** out_attributes)
{
	return Error(parser, "%s not implemented", __func__);
}

// Eats any number of modifier keywords. Which modifiers are valid where is checked by the caller.
static bool ParseModifiers(Parser& parser, uint32* out_modifiers)
{
	uint32 modifiers = 0;
	for (;;)
	{
		TRY(LexToken(parser));

		uint32 modifier = 0;
		switch (parser.token.token_type)
		{
		case TokenType::KW_CONST: modifier = MODIFIER_CONST; break;
		case TokenType::KW_STATIC: modifier = MODIFIER_STATIC; break;
		case TokenType::KW_EXTERN: modifier = MODIFIER_EXTERN; break;
		case TokenType::KW_UNIFORM: modifier = MODIFIER_UNIFORM; break;
		case TokenType::KW_EXPORT: modifier = MODIFIER_EXPORT; break;
		case TokenType::KW_INLINE: modifier = MODIFIER_INLINE; break;
		case TokenType::KW_GROUPSHARED: modifier = MODIFIER_GROUPSHARED; break;
		case TokenType::KW_GLOBALLYCOHERENT: modifier = MODIFIER_GLOBALLYCOHERENT; break;
		case TokenType::KW_PRECISE: modifier = MODIFIER_PRECISE; break;
		case TokenType::KW_ROW_MAJOR: modifier = MODIFIER_ROW_MAJOR; break;
		case TokenType::KW_COLUMN_MAJOR: modifier = MODIFIER_COLUMN_MAJOR; break;
		case TokenType::KW_SNORM: modifier = MODIFIER_SNORM; break;
		case TokenType::KW_UNORM: modifier = MODIFIER_UNORM; break;
		case TokenType::KW_IN: modifier = MODIFIER_IN; break;
		case TokenType::KW_OUT: modifier = MODIFIER_OUT; break;
		case TokenType::KW_INOUT: modifier = MODIFIER_INOUT; break;
		case TokenType::KW_NOINTERPOLATION: modifier = MODIFIER_NOINTERPOLATION; break;
		case TokenType::KW_NOPERSPECTIVE: modifier = MODIFIER_NOPERSPECTIVE; break;
		case TokenType::KW_CENTROID: modifier = MODIFIER_CENTROID; break;
		default: break;
		}
		if (!modifier)
			break;

		if (modifiers & modifier)
			return Error(parser, "duplicate modifier '%.*s'", (int)(parser.token.end - parser.token.start), parser.token.start);
		modifiers |= modifier;
		EatToken(parser);
	}

	*out_modifiers = modifiers;
	return true;
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
	TRY(LexToken(parser));

	if (parser.token.token_type == TokenType::SEMICOLON)
	{
		EatToken(parser);
		return true;
	}

	if (parser.token.token_type == TokenType::KW_STRUCT)
		return ParseStruct(parser);

	if (parser.token.token_type == TokenType::KW_TYPEDEF)
		return ParseTypedef(parser);

	// modifiers* type name, followed by either '(' for a function or the rest of a variable declarator list.
	Node* attributes = nullptr;
	if (parser.token.token_type == TokenType::LEFT_BRACKET)
		TRY(ParseAttributes(parser, &attributes));

	uint32 modifiers = 0;
	TRY(ParseModifiers(parser, &modifiers));

	Node* type = nullptr;
	TRY(ParseType(parser, &type));

	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::IDENTIFIER)
		return UnexpectedToken(parser);
	Token name = parser.token;
	EatToken(parser);

	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::LEFT_PAREN)
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
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::END_OF_FILE)
			return true;
		TRY(ParseTopLevel(parser));
	}
}

}
