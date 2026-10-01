#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Atom.hpp>
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

// Layouts below: "child" is ANode::child, following entries are chained via ANode::next.
// Any node's children may start with attributes, each an arbitrary expression with usage ATTRIBUTE.
enum class NodeType : uint8
{
	NONE = 0,

	// declarations
	CONST,     // child: expression, e.g. name: type = value
	STRUCT,    // name, child: members (statements, MEMBER)
	FUNCTION,  // AFunction
	PARAMETER, // name, child: type

	// statements
	BLOCK,  // child: statements
	RETURN, // child: value, or none

	// expressions
	NUMBER,     // ANumber
	BOOL,       // ABool
	IDENTIFIER, // name
	INIT_LIST,  // child: elements
	UNARY,      // AOperator, op: operator, child: operand
	POSTFIX,    // AOperator, op: operator, child: operand
	BINARY,     // AOperator, op: operator (including assignments and ':' declarations), child: left, right
	CALL,       // child: callee (CALLEE), arguments (ARGUMENT)...
	MEMBER,     // name: member (including swizzles), child: object
	INDEX,      // child: object, index
	ARRAY,      // [size]element, child: size, element
	IF,         // AIf
};

// How a node relates to its parent. Set by the parent.
enum class Usage : uint8
{
	NONE = 0,
	ATTRIBUTE,
	CALLEE,
	ARGUMENT,
	MEMBER,
};

struct ANode
{
	NodeType type = NodeType::NONE;
	Usage usage = Usage::NONE;
	Atom name = Atom::NONE;
	ANode* child = nullptr;
	ANode* next = nullptr;
};

struct ANumber : ANode
{
	char* text = nullptr; // as written. Parsed once the expected type is known
};

struct ABool : ANode
{
	bool value = false;
};

struct AOperator : ANode
{
	TokenType op = TokenType::NONE;
};

struct AFunction : ANode
{
	ANode* params = nullptr;      // PARAMETERs
	ANode* return_type = nullptr; // expression, nullptr if omitted
	ANode* body = nullptr;        // BLOCK
};

struct AIf : ANode
{
	ANode* condition = nullptr;
	ANode* then = nullptr;      // BLOCK or expression
	ANode* otherwise = nullptr; // BLOCK or expression, nullptr if no else
};

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
	ANode* payload = nullptr; // ARRAY: the size
};

struct Parser
{
	char* source = nullptr;
	char* head = nullptr;
	char error_buffer[256] = {}; // temp
	Token token = {};
	Arena* arena = nullptr;

	// Expression parser stacks, shared by nested expressions. Each ParseExpression only touches entries above where it started.
	std::vector<ANode*> operands;
	std::vector<PendingOp> operators;
	uint32 depth = 0; // expression and statement nesting, bounded so untrusted input can't overflow the stack
};

bool LexToken(Parser& parser);
void EatToken(Parser& parser);

// Parses a whole source file. out_declarations receives the first top-level declaration, the rest are chained via next.
// Returns false on the first error, with the message in parser.error_buffer.
bool Parse(Parser& parser, ANode** out_declarations);

Slice<uint8> CompileShader(const char* source);

}
