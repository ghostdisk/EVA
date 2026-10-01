#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Atom.hpp>
#include <EVA/Core/Error.hpp>
#include <EVA/Core/StringBuilder.hpp>
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

// The token as written for operators and keywords, otherwise a description like "identifier".
// Keep in sync with TokenType (Script_Dump.cpp).
ZTStringView TokenToString(TokenType token_type);

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

	MODULE, // a whole source file, its children are the declarations

	// declarations
	CONST,
	STRUCT,
	FUNCTION,
	PARAMETER,
	FIELD,
	VARIABLE, // name: type, made from a ':' expression by the resolver

	// statements
	BLOCK,
	RETURN,

	// expressions
	NUMBER,
	BOOL,
	IDENTIFIER,
	REFERENCE,      // an IDENTIFIER the resolver found the declaration of
	TYPE_REFERENCE, // an IDENTIFIER the resolver found to name a built-in type
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
ZTStringView NodeTypeToString(NodeType type);

// How a node relates to its parent, set by the parent. Children are looked up by usage, not position.
enum class Usage : uint8
{
	NONE = 0,
	ROOT, // parsed on its own rather than as part of a parent, e.g. a lone expression in a test
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
ZTStringView UsageToString(Usage usage);

struct Scope;
struct Type;

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
		Scope* scope;         // MODULE, FUNCTION, BLOCK: set by the resolver. A function shares its body's scope
		Node* target;         // REFERENCE: the declaration
		Type* target_type;    // TYPE_REFERENCE
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

extern uint32 RECURSION_LIMIT;

struct Parser
{
	char* source = nullptr;
	char* head = nullptr;
	Token token = {};
	Arena* arena = nullptr;           // tokens and the AST
	Arena* error_arena = nullptr;     // errors and their messages, which can outlive the AST
	std::vector<ScriptError*> errors; // allocated in error_arena

	// Expression parser stacks, shared by nested expressions. Each ParseExpression only touches entries above where it started.
	std::vector<Node*> operands;
	std::vector<PendingOp> operators;

	uint32 recursion_depth = 0;
};

ScriptError* EmitError(Parser& parser, const char* format, ...);

bool LexToken(Parser& parser);
void EatToken(Parser& parser);

// Parses a whole source file into a MODULE node, whose children are the declarations.
// Returns false on the first error, which is added to parser.errors.
bool Parse(Parser& parser, Node** out_module);

Node* ParseExpression(Parser& parser);
Node* ParseStatement(Parser& parser);

enum class TypeKind : uint8
{
	PRIMITIVE,
	VECTOR,
	MATRIX,
};

// Base of the type structs, one per TypeKind.
// Types are unique: there's only ever one instance of e.g. float2, so they can be compared by pointer. Types built from
// other types, like function types later, have to be looked up in a cache before making a new one.
struct Type
{
	TypeKind kind = TypeKind::PRIMITIVE;
	Atom name = Atom::NONE; // HLSL style, e.g. float4. Also the type's name in the global scope
	uint32 size = 0;        // in bytes
	uint32 alignment = 1; // in bytes. Buffer layout rules are applied on top of this
};

enum class PrimitiveKind : uint8
{
	VOID,
	BOOL,
	SIGNED,
	UNSIGNED,
	FLOAT,
};

struct PrimitiveType : Type
{
	PrimitiveKind primitive_kind = PrimitiveKind::VOID;

	PrimitiveType() { kind = TypeKind::PRIMITIVE; }
};

struct VectorType : Type
{
	PrimitiveType* element = nullptr;
	uint32 count = 0;

	VectorType() { kind = TypeKind::VECTOR; }
};

struct MatrixType : Type
{
	PrimitiveType* element = nullptr;
	uint32 rows = 0;
	uint32 columns = 0;

	MatrixType() { kind = TypeKind::MATRIX; }
};

// A name in a scope, for either a declaration or a built-in type. Exactly one of node and type is set.
struct Definition
{
	Atom name = Atom::NONE;
	Node* node = nullptr;
	Type* type = nullptr;
	Definition* next = nullptr;
};

// Names declared in a module, function or block. Inner scopes can shadow names from their parents.
struct Scope
{
	Scope* parent = nullptr;     // nullptr for the global scope
	Definition* first = nullptr; // a list for now, scopes are small
};

// The built-in types and the global scope naming them.
struct Context
{
	Arena* arena = nullptr; // the types and the global scope
	Scope* global_scope = nullptr;
};

// Allocates the context's types and global scope in arena, which has to live as long as the context. Usually an arena
// of its own; a short-lived context can share a temporary one.
void InitContext(Context& context, Arena* arena);

struct Resolver
{
	Context* context = nullptr;
	Arena* arena = nullptr;           // the module's scopes, for one compile
	Arena* error_arena = nullptr;     // errors and their messages
	std::vector<ScriptError*> errors; // allocated in error_arena
	Scope* scope = nullptr;           // the current one
	uint32 recursion_depth = 0;
};

ScriptError* EmitError(Resolver& resolver, const char* format, ...);

// Turns identifiers into REFERENCEs to their declarations, or TYPE_REFERENCEs to built-in types, and gives MODULE,
// FUNCTION and BLOCK nodes their scope. The module's scope sits under the context's global scope.
// Functions and structs can be referenced from anywhere in their scope, everything else only after it's declared.
// Unknown names stay IDENTIFIERs. Errors don't stop resolving the rest of the tree. Returns whether there were none.
bool Resolve(Resolver& resolver, Node* module);

// Bounds recursion so untrusted input can't overflow the stack. Goes at the start of every function that can end up
// calling itself, directly or through others; they share owner.recursion_depth, so mutual recursion counts too.
// owner is a Parser or Resolver. Returns false / nullptr from the calling function past RECURSION_LIMIT.
#define CHECK_RECURSION(owner)                       \
	(owner).recursion_depth++;                       \
	DEFER((owner).recursion_depth--);                \
	if ((owner).recursion_depth > RECURSION_LIMIT)   \
	{                                                \
		EmitError(owner, "nested too deeply");       \
		return {};                                   \
	}

void DumpNode(Node* node, Arena* arena, int indent = 0);

// Appends node and its subtree on one line, as compactly as possible while keeping everything a node holds:
// ([USAGE]TYPE name payload children...), e.g. a + b is ([ROOT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)).
// A REFERENCE's payload is its target's node type: ([CALLEE]REFERENCE f -> FUNCTION), a TYPE_REFERENCE's is the type's
// name: ([TYPE]TYPE_REFERENCE float4 -> float4).
// Meant for tests and debugging
void SerializeNode(StringBuilder& builder, Node* node);

struct CompileShaderResult
{
	Slice<uint8> data;
	Slice<ScriptError*> errors; // empty on success
};

// Everything in the result is allocated in arena, so the caller decides how long it lives. The AST, the context and
// other intermediate data use an arena of their own, destroyed before returning.
CompileShaderResult CompileShader(Arena* arena, ZTStringView source);

}
