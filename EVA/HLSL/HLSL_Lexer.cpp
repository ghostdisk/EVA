#include <EVA/HLSL/HLSL.hpp>
#include <string.h>

namespace EVA::HLSL
{

struct Keyword
{
	TokenType token_type;
	char keyword[18];
};

struct MultiCharOperator
{
	TokenType token_type;
	char op[4];
};

static Keyword keywords[] = {
	{ TokenType::KW_STRUCT, "struct" },
	{ TokenType::KW_ENUM, "enum" },
	{ TokenType::KW_CBUFFER, "cbuffer" },
	{ TokenType::KW_NAMESPACE, "namespace" },
	{ TokenType::KW_TYPEDEF, "typedef" },
	{ TokenType::KW_USING, "using" },
	{ TokenType::KW_TEMPLATE, "template" },
	{ TokenType::KW_TYPENAME, "typename" },
	{ TokenType::KW_OPERATOR, "operator" },

	{ TokenType::KW_IF, "if" },
	{ TokenType::KW_ELSE, "else" },
	{ TokenType::KW_FOR, "for" },
	{ TokenType::KW_WHILE, "while" },
	{ TokenType::KW_DO, "do" },
	{ TokenType::KW_SWITCH, "switch" },
	{ TokenType::KW_CASE, "case" },
	{ TokenType::KW_DEFAULT, "default" },
	{ TokenType::KW_BREAK, "break" },
	{ TokenType::KW_CONTINUE, "continue" },
	{ TokenType::KW_RETURN, "return" },
	{ TokenType::KW_DISCARD, "discard" },

	{ TokenType::KW_TRUE, "true" },
	{ TokenType::KW_FALSE, "false" },
	{ TokenType::KW_THIS, "this" },
	{ TokenType::KW_SIZEOF, "sizeof" },

	{ TokenType::KW_CONST, "const" },
	{ TokenType::KW_STATIC, "static" },
	{ TokenType::KW_EXTERN, "extern" },
	{ TokenType::KW_UNIFORM, "uniform" },
	{ TokenType::KW_EXPORT, "export" },
	{ TokenType::KW_INLINE, "inline" },
	{ TokenType::KW_GROUPSHARED, "groupshared" },
	{ TokenType::KW_GLOBALLYCOHERENT, "globallycoherent" },
	{ TokenType::KW_PRECISE, "precise" },
	{ TokenType::KW_ROW_MAJOR, "row_major" },
	{ TokenType::KW_COLUMN_MAJOR, "column_major" },
	{ TokenType::KW_SNORM, "snorm" },
	{ TokenType::KW_UNORM, "unorm" },

	{ TokenType::KW_IN, "in" },
	{ TokenType::KW_OUT, "out" },
	{ TokenType::KW_INOUT, "inout" },

	{ TokenType::KW_NOINTERPOLATION, "nointerpolation" },
	{ TokenType::KW_NOPERSPECTIVE, "noperspective" },
	{ TokenType::KW_CENTROID, "centroid" },

	{ TokenType::KW_REGISTER, "register" },
	{ TokenType::KW_PACKOFFSET, "packoffset" },
};

// Longest first, so the first prefix match is the longest one.
static MultiCharOperator multi_char_operators[] = {
	{ TokenType::SHIFT_LEFT_ASSIGN, "<<=" },
	{ TokenType::SHIFT_RIGHT_ASSIGN, ">>=" },

	{ TokenType::ADD_ASSIGN, "+=" },
	{ TokenType::SUBTRACT_ASSIGN, "-=" },
	{ TokenType::MULTIPLY_ASSIGN, "*=" },
	{ TokenType::DIVIDE_ASSIGN, "/=" },
	{ TokenType::MODULO_ASSIGN, "%=" },
	{ TokenType::BIT_AND_ASSIGN, "&=" },
	{ TokenType::BIT_OR_ASSIGN, "|=" },
	{ TokenType::BIT_XOR_ASSIGN, "^=" },
	{ TokenType::INCREMENT, "++" },
	{ TokenType::DECREMENT, "--" },
	{ TokenType::SHIFT_LEFT, "<<" },
	{ TokenType::SHIFT_RIGHT, ">>" },
	{ TokenType::EQUAL, "==" },
	{ TokenType::NOT_EQUAL, "!=" },
	{ TokenType::LESS_EQUAL, "<=" },
	{ TokenType::GREATER_EQUAL, ">=" },
	{ TokenType::LOGICAL_AND, "&&" },
	{ TokenType::LOGICAL_OR, "||" },
	{ TokenType::SCOPE, "::" },
};

static bool IsSingleCharOperator(char ch)
{
	return ch == ';' || ch == ',' || ch == '+' || ch == '-' || ch == '=' ||
		   ch == '*' || ch == '/' || ch == '%' || ch == '&' || ch == '|' ||
		   ch == '^' || ch == '~' || ch == '!' || ch == '?' || ch == ':' ||
		   ch == '.' || ch == '<' || ch == '>' || ch == '(' || ch == ')' ||
		   ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '#';
}

static bool IsMultiCharOperatorStart(char ch)
{
	return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
		   ch == '&' || ch == '|' || ch == '^' || ch == '<' || ch == '>' ||
		   ch == '=' || ch == '!' || ch == ':';
}

static bool IsLetter(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static bool IsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static bool IsDigit(char ch)
{
	return ch >= '0' && ch <= '9';
}

static bool SkipWhitespace(Lexer& lexer)
{
	char* start = lexer.head;
	while (IsWhitespace(*lexer.head))
	{
		lexer.head++;
	}
	return lexer.head != start;
}

static bool SkipComments(Lexer& lexer)
{
	char* head = lexer.head;
	if (head[0] != '/')
		return false;

	if (head[1] == '/')
	{
		head += 2;
		while (*head && *head != '\n')
			head++;
		lexer.head = head;
		return true;
	}

	if (head[1] == '*')
	{
		head += 2;
		while (*head && !(head[0] == '*' && head[1] == '/'))
			head++;
		if (!*head)
			return false; // unterminated, left in place for LexToken to report
		lexer.head = head + 2;
		return true;
	}

	return false;
}

bool LexToken(Lexer& lexer)
{
	if (lexer.head == lexer.token.start)
	{
		// token alredy lexed but not eaten, no need to lex it twice.
		return true;
	}

	while (SkipWhitespace(lexer) || SkipComments(lexer))
	{
	}

	char ch = *lexer.head;

	if (ch == '\0')
	{
		lexer.token = Token{
			.token_type = TokenType::END_OF_FILE,
			.start = lexer.head,
			.end = lexer.head,
		};
		return true;
	}

	if (ch == '/' && lexer.head[1] == '*')
	{
		snprintf(lexer.error_buffer, sizeof(lexer.error_buffer), "unterminated block comment");
		return false;
	}

	// Checked before operators so ".5" isn't lexed as DOT.
	if (IsDigit(ch) || (ch == '.' && IsDigit(lexer.head[1])))
	{
		char* end = lexer.head + 1;
		while (IsLetter(*end) || IsDigit(*end) || *end == '.' ||
			   ((*end == '+' || *end == '-') && (end[-1] == 'e' || end[-1] == 'E')))
			end++;

		lexer.token = Token{
			.token_type = TokenType::NUMBER,
			.start = lexer.head,
			.end = end,
		};
		return true;
	}

	if (IsSingleCharOperator(ch))
	{
		if (IsMultiCharOperatorStart(ch))
		{
			for (const MultiCharOperator& op : multi_char_operators)
			{
				uint32 length = (uint32)strlen(op.op);
				if (strncmp(lexer.head, op.op, length) == 0)
				{
					lexer.token = Token{
						.token_type = op.token_type,
						.start = lexer.head,
						.end = lexer.head + length,
					};
					return true;
				}
			}
		}

		lexer.token = Token{
			.token_type = (TokenType)ch,
			.start = lexer.head,
			.end = lexer.head + 1,
		};
		return true;
	}

	if (IsLetter(ch))
	{
		lexer.token = Token{
			.token_type = TokenType::IDENTIFIER,
			.start = lexer.head,
		};

		char* end = lexer.head + 1;
		while (IsLetter(*end) || IsDigit(*end))
			end++;
		lexer.token.end = end;

		uint32 length = (uint32)(end - lexer.head);
		for (const Keyword& keyword : keywords)
		{
			if (length < sizeof(keyword.keyword) && keyword.keyword[length] == '\0' &&
				memcmp(keyword.keyword, lexer.head, length) == 0)
			{
				lexer.token.token_type = keyword.token_type;
				break;
			}
		}

		return true;
	}

	// ...

	snprintf(lexer.error_buffer, sizeof(lexer.error_buffer), "unexpected character %d", (int)ch);
	return false;
}

void EatToken(Lexer& lexer)
{
	lexer.head = lexer.token.end;
	lexer.token = {};
}

}
