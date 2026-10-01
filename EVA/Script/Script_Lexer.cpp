#include <EVA/Script/Script.hpp>
#include <string.h>

namespace EVA::Script
{

struct Keyword
{
	TokenType token_type;
	char keyword[9];
};

struct MultiCharOperator
{
	TokenType token_type;
	char op[4];
};

static Keyword keywords[] = {
	{ TokenType::KW_CONST, "const" },
	{ TokenType::KW_STRUCT, "struct" },
	{ TokenType::KW_FUNCTION, "function" },

	{ TokenType::KW_IF, "if" },
	{ TokenType::KW_ELSE, "else" },
	{ TokenType::KW_RETURN, "return" },

	{ TokenType::KW_TRUE, "true" },
	{ TokenType::KW_FALSE, "false" },
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
};

static bool IsSingleCharOperator(char ch)
{
	return ch == ';' || ch == ',' || ch == '+' || ch == '-' || ch == '=' ||
		   ch == '*' || ch == '/' || ch == '%' || ch == '&' || ch == '|' ||
		   ch == '^' || ch == '~' || ch == '!' || ch == ':' || ch == '.' ||
		   ch == '<' || ch == '>' || ch == '(' || ch == ')' || ch == '[' ||
		   ch == ']' || ch == '{' || ch == '}' || ch == '@';
}

static bool IsMultiCharOperatorStart(char ch)
{
	return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
		   ch == '&' || ch == '|' || ch == '^' || ch == '<' || ch == '>' ||
		   ch == '=' || ch == '!';
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

static bool SkipWhitespace(Parser& parser)
{
	char* start = parser.head;
	while (IsWhitespace(*parser.head))
	{
		parser.head++;
	}
	return parser.head != start;
}

static bool SkipComments(Parser& parser)
{
	char* head = parser.head;
	if (head[0] != '/')
		return false;

	if (head[1] == '/')
	{
		head += 2;
		while (*head && *head != '\n')
			head++;
		parser.head = head;
		return true;
	}

	if (head[1] == '*')
	{
		head += 2;
		while (*head && !(head[0] == '*' && head[1] == '/'))
			head++;
		if (!*head)
			return false; // unterminated, left in place for LexToken to report
		parser.head = head + 2;
		return true;
	}

	return false;
}

bool LexToken(Parser& parser)
{
	if (parser.head == parser.token.start)
	{
		// token alredy lexed but not eaten, no need to lex it twice.
		return true;
	}

	while (SkipWhitespace(parser) || SkipComments(parser))
	{
	}

	char ch = *parser.head;

	if (ch == '\0')
	{
		parser.token = Token{
			.token_type = TokenType::END_OF_FILE,
			.start = parser.head,
			.end = parser.head,
		};
		return true;
	}

	if (ch == '/' && parser.head[1] == '*')
	{
		snprintf(parser.error_buffer, sizeof(parser.error_buffer), "unterminated block comment");
		return false;
	}

	// Checked before operators so ".5" isn't lexed as DOT.
	if (IsDigit(ch) || (ch == '.' && IsDigit(parser.head[1])))
	{
		char* end = parser.head + 1;
		while (IsLetter(*end) || IsDigit(*end) || *end == '.' ||
			   ((*end == '+' || *end == '-') && (end[-1] == 'e' || end[-1] == 'E')))
			end++;

		parser.token = Token{
			.token_type = TokenType::NUMBER,
			.start = parser.head,
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
				if (strncmp(parser.head, op.op, length) == 0)
				{
					parser.token = Token{
						.token_type = op.token_type,
						.start = parser.head,
						.end = parser.head + length,
					};
					return true;
				}
			}
		}

		parser.token = Token{
			.token_type = (TokenType)ch,
			.start = parser.head,
			.end = parser.head + 1,
		};
		return true;
	}

	if (IsLetter(ch))
	{
		parser.token = Token{
			.token_type = TokenType::IDENTIFIER,
			.start = parser.head,
		};

		char* end = parser.head + 1;
		while (IsLetter(*end) || IsDigit(*end))
			end++;
		parser.token.end = end;

		uint32 length = (uint32)(end - parser.head);
		for (const Keyword& keyword : keywords)
		{
			if (length < sizeof(keyword.keyword) && keyword.keyword[length] == '\0' &&
				memcmp(keyword.keyword, parser.head, length) == 0)
			{
				parser.token.token_type = keyword.token_type;
				break;
			}
		}

		if (parser.token.token_type == TokenType::IDENTIFIER)
			parser.token.atom = GetAtom(StringView(parser.head, length));

		return true;
	}

	// ...

	snprintf(parser.error_buffer, sizeof(parser.error_buffer), "unexpected character %d", (int)ch);
	return false;
}

void EatToken(Parser& parser)
{
	parser.head = parser.token.end;
	parser.token = {};
}

}
