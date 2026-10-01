#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Atom.hpp>
#include <EVA/Core/Error.hpp>
#include <vector>

namespace EVA::Script
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
	AT = '@',

	IDENTIFIER = 128,
	NUMBER = 129,

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

	// keywords:
	KW_CONST,
	KW_STRUCT,
	KW_FUNCTION,

	KW_IF,
	KW_ELSE,
	KW_RETURN,

	KW_TRUE,
	KW_FALSE,
};

struct Token
{
	TokenType token_type = TokenType::NONE;
	char* start = nullptr;
	char* end = nullptr;
	Atom atom = Atom::NONE; // IDENTIFIER only
};

enum class NodeType : uint8
{
	NONE = 0,

	// declarations
	CONST,
	STRUCT,
	FUNCTION,
	PARAMETER,
	FIELD,

	// statements
	BLOCK,
	RETURN,

	// expressions
	NUMBER,
	BOOL,
	IDENTIFIER,
	INIT_LIST,
	UNARY,
	POSTFIX,
	BINARY,
	CALL,
	MEMBER,
	INDEX,
	ARRAY_TYPE,
	IF,
};

// Keep in sync with NodeType (Script_Dump.cpp).
const char* NodeTypeName(NodeType type);

// How a node relates to its parent, set by the parent. Children are looked up by usage, not position.
enum class Usage : uint8
{
	NONE = 0,
	DECLARATION,
	ATTRIBUTE,
	VALUE,
	MEMBER,
	PARAMETER,
	RETURN_TYPE,
	BODY,
	TYPE,
	STATEMENT,
	ELEMENT,
	SIZE,
	OPERAND,
	LEFT,
	RIGHT,
	CALLEE,
	ARGUMENT,
	OBJECT,
	INDEX,
	CONDITION,
	THEN,
	ELSE,
};

// Keep in sync with Usage (Script_Dump.cpp).
const char* UsageName(Usage usage);

struct Node
{
	NodeType type = NodeType::NONE;
	Usage usage = Usage::NONE;
	Atom name = Atom::NONE;
	union
	{
		char* text = nullptr; // NUMBER: as written. Parsed once the expected type is known
		bool value;           // BOOL
		TokenType op;         // UNARY, POSTFIX, BINARY
	};
	Node* child = nullptr; // first child, the rest are chained via next
	Node* next = nullptr;
};

// The first child with the given usage, or nullptr.
inline Node* FindChild(Node* node, Usage usage)
{
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->usage == usage)
			return child;
	}
	return nullptr;
}

enum class OpKind : uint8
{
	PREFIX,
	INFIX,
	ARRAY,
};

// An operator waiting on the expression parser's operator stack.
struct PendingOp
{
	TokenType op = TokenType::NONE;
	OpKind kind = OpKind::INFIX;
	Node* payload = nullptr; // ARRAY: the size
};

struct ScriptError : Error
{
	ScriptError() { error_family = ErrorFamily::SCRIPT_ERROR; }
};

struct Parser
{
	char* source = nullptr;
	char* head = nullptr;
	Token token = {};
	Arena* arena = nullptr;
	std::vector<ScriptError*> errors; // allocated in arena

	// Expression parser stacks, shared by nested expressions. Each ParseExpression only touches entries above where it started.
	std::vector<Node*> operands;
	std::vector<PendingOp> operators;
	uint32 depth = 0; // expression and statement nesting, bounded so untrusted input can't overflow the stack
};

// Allocates an error in parser.arena with a printf formatted message and adds it to parser.errors.
// Returns the error so callers can attach more data, not false: write EmitError(...); return false;
ScriptError* EmitError(Parser& parser, const char* format, ...);

bool LexToken(Parser& parser);
void EatToken(Parser& parser);

// Parses a whole source file. out_declarations receives the first top-level declaration, the rest are chained via next.
// Returns false on the first error, which is added to parser.errors.
bool Parse(Parser& parser, Node** out_declarations);

void DumpNode(Node* node, Arena* arena, int indent = 0);

Slice<uint8> CompileShader(const char* source);

}
