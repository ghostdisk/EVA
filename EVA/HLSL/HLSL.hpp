#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA::HLSL
{

enum class TokenType : uint8
{
	NONE = 0,
	END_OF_FILE = 0,

	// 0 to 127 are reserved for single char operators that match their ascii:

	SEMICOLON = ';',
	COMMA = ',',
	PLUS = '+',
	MINUS = '-',
	EQUALS = '=',
	ASTERISK = '*',
	SLASH = '/',
	PERCENT = '%',
	AMPERSAND = '&',
	PIPE = '|',
	CARET = '^',
	TILDE = '~',
	EXCLAMATION = '!',
	QUESTION = '?',
	COLON = ':',
	DOT = '.',
	LESS = '<',
	GREATER = '>',
	LEFT_PAREN = '(',
	RIGHT_PAREN = ')',
	LEFT_BRACKET = '[',
	RIGHT_BRACKET = ']',
	LEFT_BRACE = '{',
	RIGHT_BRACE = '}',
	HASH = '#',

	IDENTIFIER = 128,
	NUMBER = 129,
	STRING,

	// operators:
	ADD_ASSIGN,
	SUBTRACT_ASSIGN,
	MULTIPLY_ASSIGN,
	DIVIDE_ASSIGN,
	MODULO_ASSIGN,
	BIT_AND_ASSIGN,
	BIT_OR_ASSIGN,
	BIT_XOR_ASSIGN,
	SHIFT_LEFT_ASSIGN,
	SHIFT_RIGHT_ASSIGN,
	INCREMENT,
	DECREMENT,
	SHIFT_LEFT,
	SHIFT_RIGHT,
	EQUAL,
	NOT_EQUAL,
	LESS_EQUAL,
	GREATER_EQUAL,
	LOGICAL_AND,
	LOGICAL_OR,
	SCOPE,

	// keywords:
	KW_STRUCT,
	KW_ENUM,
	KW_CBUFFER,
	KW_NAMESPACE,
	KW_TYPEDEF,
	KW_USING,
	KW_TEMPLATE,
	KW_TYPENAME,
	KW_OPERATOR,

	KW_IF,
	KW_ELSE,
	KW_FOR,
	KW_WHILE,
	KW_DO,
	KW_SWITCH,
	KW_CASE,
	KW_DEFAULT,
	KW_BREAK,
	KW_CONTINUE,
	KW_RETURN,
	KW_DISCARD,

	KW_TRUE,
	KW_FALSE,
	KW_THIS,
	KW_SIZEOF,

	// storage and type modifiers
	KW_CONST,
	KW_STATIC,
	KW_EXTERN,
	KW_UNIFORM,
	KW_EXPORT,
	KW_INLINE,
	KW_GROUPSHARED,
	KW_GLOBALLYCOHERENT,
	KW_PRECISE,
	KW_ROW_MAJOR,
	KW_COLUMN_MAJOR,
	KW_SNORM,
	KW_UNORM,

	// parameter modifiers
	KW_IN,
	KW_OUT,
	KW_INOUT,

	// interpolation modifiers. linear and sample are contextual.
	KW_NOINTERPOLATION,
	KW_NOPERSPECTIVE,
	KW_CENTROID,

	// only valid after ':' but never usable as identifiers
	KW_REGISTER,
	KW_PACKOFFSET,
};

struct Token
{
	TokenType token_type = TokenType::NONE;
	char* start = nullptr;
	char* end = nullptr;
};

struct Lexer
{
	char* source = nullptr;
	char* head = nullptr;
	char error_buffer[256] = {}; // temp
	Token token = {};
};

bool LexToken(Lexer& lexer);
void EatToken(Lexer& lexer);

Slice<uint8> Compile(const char* source);

}
