#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <vector>

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

// Layouts below: "child" is Node::child, following entries are chained via Node::next.
enum class NodeType : uint8
{
	NONE = 0,
	EMPTY, // placeholder, e.g. the size of an unsized array []

	// declarations
	MODULE,   // child: declarations
	VARIABLE, // AVariable, flags: modifiers
	FUNCTION, // AFunction, flags: modifiers
	STRUCT,   // name, child: members (VARIABLE / FUNCTION)
	TYPEDEF,  // ATypedef
	ATTRIBUTE, // name, child: arguments

	// statements
	BLOCK,    // child: statements
	IF,       // AIf
	FOR,      // AFor
	WHILE,    // AWhile
	DO_WHILE, // AWhile
	SWITCH,   // ASwitch, child: CASEs
	CASE,     // ACase, child: statements
	RETURN,   // child: value, or none
	BREAK,
	CONTINUE,
	DISCARD,

	// expressions
	NUMBER,     // name: text
	BOOL,       // name: true/false
	STRING,     // name: text
	IDENTIFIER, // name
	TEMPLATE,   // child: template name, arguments...
	INIT_LIST,  // child: elements
	UNARY,      // AOperator, op: operator, child: operand
	POSTFIX,    // AOperator, op: operator, child: operand
	BINARY,     // AOperator, op: operator (including assignments), child: left, right
	TERNARY,    // child: condition, then, else
	CALL,       // child: callee, arguments...
	CAST,       // child: type, operand
	MEMBER,     // name: member name (including swizzles), child: object
	INDEX,      // child: object, index
};

enum Modifier : uint32
{
	MODIFIER_CONST = 1 << 0,
	MODIFIER_STATIC = 1 << 1,
	MODIFIER_EXTERN = 1 << 2,
	MODIFIER_UNIFORM = 1 << 3,
	MODIFIER_EXPORT = 1 << 4,
	MODIFIER_INLINE = 1 << 5,
	MODIFIER_GROUPSHARED = 1 << 6,
	MODIFIER_GLOBALLYCOHERENT = 1 << 7,
	MODIFIER_PRECISE = 1 << 8,
	MODIFIER_ROW_MAJOR = 1 << 9,
	MODIFIER_COLUMN_MAJOR = 1 << 10,
	MODIFIER_SNORM = 1 << 11,
	MODIFIER_UNORM = 1 << 12,
	MODIFIER_IN = 1 << 13,
	MODIFIER_OUT = 1 << 14,
	MODIFIER_INOUT = MODIFIER_IN | MODIFIER_OUT,
	MODIFIER_NOINTERPOLATION = 1 << 15,
	MODIFIER_NOPERSPECTIVE = 1 << 16,
	MODIFIER_CENTROID = 1 << 17,
};

struct Node
{
	NodeType type = NodeType::NONE;
	char name[16] = {};
	uint32 flags = 0;
	Node* child = nullptr;
	Node* next = nullptr;
};

struct AOperator : Node
{
	TokenType op = TokenType::NONE;
};

struct AVariable : Node
{
	Node* declared_type = nullptr;  // IDENTIFIER or TEMPLATE
	Node* array_sizes = nullptr; // one expression per dimension, EMPTY for []
	char semantic[32] = {};      // empty if absent
	Node* initializer = nullptr; // expression or INIT_LIST. default value for parameters
};

struct AFunction : Node
{
	Node* attributes = nullptr;  // ATTRIBUTEs
	Node* return_type = nullptr; // IDENTIFIER or TEMPLATE
	Node* params = nullptr;      // VARIABLEs
	char semantic[32] = {};      // empty if absent
	Node* body = nullptr;        // BLOCK, nullptr for a prototype
};

struct ATypedef : Node
{
	Node* declared_type = nullptr;  // IDENTIFIER or TEMPLATE
	Node* array_sizes = nullptr; // one expression per dimension
};

struct AIf : Node
{
	Node* attributes = nullptr;
	Node* condition = nullptr;
	Node* then = nullptr;
	Node* otherwise = nullptr; // nullptr if no else
};

struct AFor : Node
{
	Node* attributes = nullptr;
	Node* init = nullptr;      // VARIABLEs or expressions, nullptr if empty
	Node* condition = nullptr; // nullptr if empty
	Node* step = nullptr;      // nullptr if empty
	Node* body = nullptr;
};

struct AWhile : Node
{
	Node* attributes = nullptr;
	Node* condition = nullptr;
	Node* body = nullptr;
};

struct ASwitch : Node
{
	Node* attributes = nullptr;
	Node* value = nullptr;
};

struct ACase : Node
{
	Node* value = nullptr; // nullptr for default
};

enum class OpKind : uint8
{
	PREFIX,
	INFIX,
	CAST,
	TERNARY,
};

// An operator waiting on the expression parser's operator stack.
struct PendingOp
{
	TokenType op = TokenType::NONE;
	OpKind kind = OpKind::INFIX;
	Node* payload = nullptr; // CAST: the type. TERNARY: the middle expression
};

struct Parser
{
	char* source = nullptr;
	char* head = nullptr;
	char error_buffer[256] = {}; // temp
	Token token = {};
	Arena* arena = nullptr;
	Node** tail = nullptr; // where the next node of the list being parsed is appended

	// Expression parser stacks, shared by nested expressions. Each ParseExpression only touches entries above where it started.
	std::vector<Node*> operands;
	std::vector<PendingOp> operators;
	uint32 depth = 0; // expression nesting, bounded so untrusted input can't overflow the stack
};

bool LexToken(Parser& parser);
void EatToken(Parser& parser);

// Parses a whole source file. out_declarations receives the first top-level declaration, the rest are chained via next.
// Returns false on the first error, with the message in parser.error_buffer.
bool Parse(Parser& parser, Node** out_declarations);

Slice<uint8> Compile(const char* source);

}
