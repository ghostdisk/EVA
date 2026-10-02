#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Atom.hpp>
#include <EVA/Core/Error.hpp>
#include <EVA/Core/StringBuilder.hpp>
#include <vector>

namespace EVA::Script
{

enum class TokenType : uint8 // NOLINT(cert-int09-c): END_OF_FILE shares 0 with NONE, operators are their ASCII
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

enum class NumberKind : uint8
{
	INTEGER,
	FLOAT, // written with a '.' or an exponent
};

// A NUMBER's value, parsed by the lexer. The typer picks its type and checks that the value fits.
struct NumberLiteral
{
	NumberKind kind = NumberKind::INTEGER;
	union
	{
		uint64 integer = 0; // INTEGER: never negative, '-' is an operator
		double f64;         // FLOAT
	};
	float f32 = 0.0f; // FLOAT: parsed separately rather than rounded from f64, so it's exact too. inf if too large
};

struct Token
{
	TokenType token_type = TokenType::NONE;
	char* start = nullptr;
	char* end = nullptr;
	Atom atom = Atom::NONE;          // IDENTIFIER only
	NumberLiteral* number = nullptr; // NUMBER only
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
	VARIABLE,   // name: type, made from a ':' expression by the resolver
	ENUM_VALUE,

	// statements
	BLOCK,
	RETURN,

	// expressions
	NUMBER,
	BOOL,
	IDENTIFIER,
	REFERENCE, // an IDENTIFIER the resolver found the definition of
	CONSTANT,  // a constant expression the typer evaluated, which it replaces
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
	DECLARED_TYPE,
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
struct Constant;

enum class ElementKind : uint8
{
	NONE = 0,
	NODE,
	TYPE,
	INTRINSIC,
	CONSTANT,
};

// Base of everything a name can refer to.
struct Element
{
	ElementKind kind = ElementKind::NONE;
};

// Of a CONST, which the typer types on first use. That can be before its turn: a struct laid out early can use a const
// as an array size.
enum class TypingState : uint8
{
	UNTYPED,
	TYPING,
	TYPED,
	FAILED,
};

struct Node : Element
{
	NodeType node_type = NodeType::NONE;
	Usage usage = Usage::NONE;
	TypingState typing_state = TypingState::UNTYPED; // CONST
	Atom name = Atom::NONE;
	Type* type = nullptr; // set by the typer: the value's type, or for a type expression the type it names
	union
	{
		NumberLiteral* number = nullptr; // NUMBER
		bool value;           // BOOL
		TokenType op;         // UNARY, POSTFIX, BINARY
		Scope* scope;         // MODULE, FUNCTION, BLOCK: set by the resolver. A function shares its body's scope
		Element* target;      // REFERENCE
		int64 enum_value;     // ENUM_VALUE
		Constant* constant;   // CONSTANT
	};
	Node* child = nullptr; // first child, the rest are chained via next
	Node* next = nullptr;

	Node() { kind = ElementKind::NODE; }
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
	ENUM,
	ARRAY,
	STRUCT,
	POINTER,  // IR only for now
	FUNCTION, // IR only for now
};

// Base of the type structs, one per TypeKind.
// Types are unique: there's only ever one instance of e.g. float2, so they can be compared by pointer. Types built from
// other types, like function types later, have to be looked up in a cache before making a new one.
struct Type : Element
{
	TypeKind type_kind = TypeKind::PRIMITIVE;
	Atom name = Atom::NONE; // HLSL style, e.g. float4. Also the type's name in the global scope
	uint32 size = 0;        // in bytes
	uint32 alignment = 1; // in bytes. Buffer layout rules are applied on top of this

	Type() { kind = ElementKind::TYPE; }
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

	PrimitiveType() { type_kind = TypeKind::PRIMITIVE; }
};

struct VectorType : Type
{
	PrimitiveType* element = nullptr;
	uint32 count = 0;

	VectorType() { type_kind = TypeKind::VECTOR; }
};

struct MatrixType : Type
{
	PrimitiveType* element = nullptr;
	uint32 rows = 0;
	uint32 columns = 0;

	MatrixType() { type_kind = TypeKind::MATRIX; }
};

struct EnumType : Type
{
	Scope* scope = nullptr; // the values, as ENUM_VALUE nodes

	EnumType() { type_kind = TypeKind::ENUM; }
};

struct ArrayType : Type
{
	Type* element = nullptr;
	uint32 length = 0;
	uint32 stride = 0; // the element's size rounded up to its alignment

	ArrayType() { type_kind = TypeKind::ARRAY; }
};

struct StructField
{
	Atom name = Atom::NONE;
	Type* type = nullptr;
	uint32 offset = 0;
	Node* declaration = nullptr; // the FIELD
};

enum class StructState : uint8
{
	DECLARED,   // by the resolver
	COMPLETING, // the typer is typing its fields
	COMPLETE,
	FAILED,
};

// Made by the resolver for each STRUCT; the typer fills in the fields, size and alignment when it first needs them.
struct StructType : Type
{
	Node* declaration = nullptr;
	Slice<StructField> fields;
	StructState state = StructState::DECLARED;

	StructType() { type_kind = TypeKind::STRUCT; }
};

// Where a pointer points. See Docs/IR.md.
enum class AddressSpace : uint8
{
	FUNCTION, // locals
	PRIVATE,  // mutable globals
	CONSTANT, // read-only globals
	INPUT,    // shader interface globals
	OUTPUT,
	MEMORY,   // scripts' linear memory. Reserved, not supported yet
};

// Keep in sync with AddressSpace (Script_Dump.cpp).
ZTStringView AddressSpaceToString(AddressSpace space);

struct PointerType : Type
{
	AddressSpace space = AddressSpace::FUNCTION;
	Type* pointee = nullptr;

	PointerType() { type_kind = TypeKind::POINTER; }
};

struct FunctionType : Type
{
	Type* return_type = nullptr; // void for none
	Slice<Type*> parameters;

	FunctionType() { type_kind = TypeKind::FUNCTION; }
};

// The values of the Semantic enum, the argument of @semantic(...).
enum class Semantic : uint8
{
	VERTEX_INDEX,
	POSITION,
};

// As written in @semantic(...). Keep in sync with Semantic (Script_Dump.cpp).
ZTStringView SemanticToString(Semantic semantic);

// The values of the ShaderStage enum, the argument of @entry(...).
enum class ShaderStage : uint8
{
	VERTEX,
	FRAGMENT,
};

// As written in @entry(...). Keep in sync with ShaderStage (Script_Dump.cpp).
ZTStringView ShaderStageToString(ShaderStage stage);

enum class IntrinsicKind : uint8
{
	NONE,

	// shader attributes:
	SEMANTIC,
	LOCATION,
	ENTRY,
};

struct Intrinsic : Element
{
	IntrinsicKind intrinsic_kind = IntrinsicKind::NONE;
	Atom name = Atom::NONE;
	Scope* argument_scope = nullptr; // where the arguments of a call to it are resolved. nullptr: the call's own scope

	Intrinsic() { kind = ElementKind::INTRINSIC; }
};

// Laid out by the type, padding zeroed.
struct Constant : Element
{
	Type* type = nullptr;
	Slice<uint8> bytes;

	Constant() { kind = ElementKind::CONSTANT; }
};

struct Definition
{
	Atom name = Atom::NONE;
	Element* element = nullptr;
	Definition* next = nullptr;
};

// Names declared in a module, function or block. Inner scopes can shadow names from their parents.
struct Scope
{
	Scope* parent = nullptr;     // nullptr for the global scope
	Definition* first = nullptr; // a list for now, scopes are small
};

enum class ContextKind : uint8
{
	SCRIPT,
	SHADER,
};

// The built-ins and the global scope naming them.
struct Context
{
	Arena* arena = nullptr; // the built-ins and the global scope
	Scope* global_scope = nullptr;

	PrimitiveType* void_type = nullptr;
	PrimitiveType* bool_type = nullptr; // not in the global scope yet, the IR uses it
	PrimitiveType* int_type = nullptr;
	PrimitiveType* uint_type = nullptr;
	PrimitiveType* float_type = nullptr;
	EnumType* semantic_type = nullptr; // SHADER only
	EnumType* stage_type = nullptr;    // SHADER only

	std::vector<ArrayType*> array_types; // see GetArrayType
	std::vector<VectorType*> vector_types;
	std::vector<PointerType*> pointer_types;
	std::vector<FunctionType*> function_types;
};

// Allocates the context's built-ins and global scope in arena, which has to live as long as the context. Usually an
// arena of its own; a short-lived context can share a temporary one.
void InitContext(Context& context, Arena* arena, ContextKind kind);

// The one array type of element and length, made in the context's arena on first use. nullptr if it would be too large.
ArrayType* GetArrayType(Context& context, Type* element, uint32 length);

// The one vector type of 2 to 4 elements, e.g. int3. The float ones are in the global scope, the others aren't yet.
VectorType* GetVectorType(Context& context, PrimitiveType* element, uint32 count);

PointerType* GetPointerType(Context& context, AddressSpace space, Type* pointee);
FunctionType* GetFunctionType(Context& context, Type* return_type, Slice<Type*> parameters);

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

// Turns identifiers into REFERENCEs to their definitions, declared in the source or built in, and gives MODULE, FUNCTION
// and BLOCK nodes their scope. The module's scope sits under the context's global scope.
// Functions and structs can be referenced from anywhere in their scope, everything else only after it's declared.
// Unknown names stay IDENTIFIERs. Errors don't stop resolving the rest of the tree. Returns whether there were none.
bool Resolve(Resolver& resolver, Node* module);

struct Typer
{
	Context* context = nullptr;
	Arena* arena = nullptr;           // the module's constants, for one compile
	Arena* error_arena = nullptr;     // errors and their messages
	std::vector<ScriptError*> errors; // allocated in error_arena
	Type* return_type = nullptr;      // of the function being typed
	uint64 constant_size = 0;         // of all constants made so far, see TOTAL_CONSTANT_SIZE_LIMIT
	uint32 recursion_depth = 0;
};

// In bytes, for one constant and for all of a module's.
extern uint32 CONSTANT_SIZE_LIMIT;
extern uint64 TOTAL_CONSTANT_SIZE_LIMIT;

ScriptError* EmitError(Typer& typer, const char* format, ...);

// Gives every expression and declaration in a resolved module its type, and every CONST its value. Errors don't stop
// typing the rest of the tree. Returns whether there were none.
// Constant expressions (a const's value, array sizes, locations) aren't typed, they're evaluated by EvaluateConstant.
bool TypeCheck(Typer& typer, Node* module);

// Constant expressions are a subset of the language evaluated at compile time, without typing their nodes: number
// literals, arithmetic on them, references to consts, indexing and member access on those, initializer lists and
// vector constructors. Anything else isn't one.
// The value of node, or nullptr with an error if it isn't a constant expression ("<what> must be a constant") or is
// invalid. expected is a hint like in the typer: literals take it, initializer lists need it. The caller checks the
// result's type. On success node becomes a CONSTANT of the value, its children dropped except for attributes.
Constant* EvaluateConstant(Typer& typer, Node* node, Type* expected, const char* what);

// EvaluateConstant, but nullptr without an error if node isn't a constant expression. Still errors for invalid ones,
// like division by zero.
Constant* TryEvaluateConstant(Typer& typer, Node* node, Type* expected);

// A constant int or uint as int64.
int64 ConstantToInteger(Constant* constant);

// Typer internals shared with EvaluateConstant.

// The scalar of a primitive or vector type, nullptr for anything else.
PrimitiveType* ComponentType(Type* type);
uint32 ComponentCount(Type* type);
bool IsNumeric(PrimitiveType* type);
bool IsInteger(PrimitiveType* type);

// The type a number literal takes where expected is wanted, nullptr with an error if its value doesn't fit.
PrimitiveType* NumberType(Typer& typer, NumberLiteral* number, Type* expected);

// Errors if node_type's (UNARY or BINARY) op isn't supported yet.
bool CheckOperator(Typer& typer, NodeType node_type, TokenType op);

// Errors if node_type's op can't be applied to operands of type.
bool CheckOperands(Typer& typer, NodeType node_type, TokenType op, Type* type);

// Counts an argument of type to vector's constructor into components, erroring if it can't be one.
bool AddConstructorArgument(Typer& typer, VectorType* vector, Type* type, uint32* components);

// Errors unless components is right for vector's constructor: all of them, or one for a splat.
bool CheckConstructorComponents(Typer& typer, VectorType* vector, uint32 components);

// The type a type expression names, also stored in the node.
Type* EvaluateType(Typer& typer, Node* node);

// Types a CONST and evaluates its value, which becomes a CONSTANT node, the first time it's needed. That can be before
// its turn: a struct laid out early can use a const as an array size.
bool TypeConst(Typer& typer, Node* node);

// Bounds recursion so untrusted input can't overflow the stack. Goes at the start of every function that can end up
// calling itself, directly or through others; they share owner.recursion_depth, so mutual recursion counts too.
// owner is a Parser, Resolver or Typer. Returns false / nullptr from the calling function past RECURSION_LIMIT.
#define CHECK_RECURSION(owner)                       \
	(owner).recursion_depth++;                       \
	DEFER((owner).recursion_depth--);                \
	if ((owner).recursion_depth > RECURSION_LIMIT)   \
	{                                                \
		EmitError(owner, "nested too deeply");       \
		return {};                                   \
	}

enum class IODirection : uint8
{
	INPUT,
	OUTPUT,
};

enum class IOKind : uint8
{
	SEMANTIC,
	LOCATION,
};

// One input or output of an entry point: a scalar or vector leaf of a parameter or the return value, which can be
// nested in structs.
struct ShaderIO
{
	IODirection direction = IODirection::INPUT;
	IOKind io_kind = IOKind::SEMANTIC;
	Semantic semantic = Semantic::VERTEX_INDEX; // SEMANTIC
	uint32 location = 0;                        // LOCATION
	Type* type = nullptr;
	Slice<uint32> path;          // INPUT: [parameter index, field index, ...]. OUTPUT: [field index, ...] into the return value
	Node* declaration = nullptr; // the PARAMETER, FIELD or RETURN_TYPE with the attribute
};

// An @entry(...) function.
struct EntryPoint
{
	ShaderStage stage = ShaderStage::VERTEX;
	Node* function = nullptr;
	Slice<ShaderIO> io;
};

// What a shader module exposes to the pipeline.
struct ShaderInterface
{
	Slice<EntryPoint> entry_points;
};

struct ShaderInterfaceBuilder
{
	Arena* arena = nullptr;           // the interface, for one compile
	Arena* error_arena = nullptr;     // errors and their messages
	std::vector<ScriptError*> errors; // allocated in error_arena
	uint32 recursion_depth = 0;
};

ScriptError* EmitError(ShaderInterfaceBuilder& builder, const char* format, ...);

// Finds the entry points of a typed shader module and flattens their parameters and return values into inputs and
// outputs, checking the semantics and locations. Returns whether there were no errors.
bool BuildShaderInterface(ShaderInterfaceBuilder& builder, Node* module, ShaderInterface* out_interface);

// e.g. float4, [3]float2, *function float4, (uint, float) -> float4. Allocated in arena when it isn't a type's own name.
ZTStringView TypeToString(Type* type, Arena* arena);

// e.g. 12, 0.5, 1000.0, 1e+40: integers in decimal, floats as the shortest text that parses back to the same double,
// always with a '.' or an exponent. Allocated in arena.
ZTStringView NumberToString(NumberLiteral* number, Arena* arena);

// e.g. 3, 0.5, (1.0, 2.0) for vectors, {1, 2} for arrays and structs. Allocated in arena.
ZTStringView ConstantToString(Constant* constant, Arena* arena);

void DumpNode(Node* node, Arena* arena, int indent = 0);

// One line per entry point and per input or output, e.g.
// vertex VSMain
//   input semantic(vertex_index) uint [0]
// Meant for tests and debugging. Allocated in arena.
ZTStringView ShaderInterfaceToString(ShaderInterface& shader_interface, Arena* arena);

// Appends node and its subtree on one line, as compactly as possible while keeping everything a node holds:
// ([USAGE]TYPE:type name payload children...), with :type once the typer has set it, e.g. a + b is ([ROOT]BINARY + ([LEFT]IDENTIFIER a) ([RIGHT]IDENTIFIER b)).
// A REFERENCE's payload is its target's node type: ([CALLEE]REFERENCE f -> FUNCTION), or for a built-in its kind and
// name: ([TYPE]REFERENCE float4 -> TYPE float4). A constant is named by its type. A CONSTANT's payload is its value.
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
