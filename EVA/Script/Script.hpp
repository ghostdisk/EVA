#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Atom.hpp>
#include <EVA/Core/Error.hpp>
#include <EVA/Core/GPUShared.hpp>
#include <EVA/Core/StringBuilder.hpp>
#include <unordered_map>
#include <vector>

#define CHECK_RECURSION(owner)                     \
	(owner).recursion_depth++;                     \
	DEFER((owner).recursion_depth--);              \
	if ((owner).recursion_depth > RECURSION_LIMIT) \
	{                                              \
		EmitError(owner, "nested too deeply");     \
		return {};                                 \
	}

namespace EVA::Script
{

struct Scope;
struct Type;
struct Constant;
struct Context;
struct Typer;
struct Generic;
using GPU::ShaderStage;

enum class TokenType : uint8 // NOLINT(cert-int09-c): END_OF_FILE shares 0 with NONE, operators are their ASCII
{
	NONE = 0,
	END_OF_FILE = 0,

	// 0 to 127: single char operators, as their ASCII
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

	KW_CONST,
	KW_LET,
	KW_STRUCT,
	KW_FUNCTION,
	KW_TYPE,

	KW_IF,
	KW_ELSE,
	KW_RETURN,

	KW_TRUE,
	KW_FALSE,
};

enum class NumberKind : uint8
{
	INTEGER,
	FLOAT,
};

struct NumberLiteral
{
	NumberKind kind = NumberKind::INTEGER;
	union
	{
		uint64 integer = 0; // never negative, '-' is an operator
		double f64;
	};
	float f32 = 0.0f; // parsed on its own rather than rounded from f64. inf if too large
};

struct Token
{
	TokenType token_type = TokenType::NONE;
	char* start = nullptr;
	char* end = nullptr;
	Atom atom = Atom::NONE;			 // IDENTIFIER
	NumberLiteral* number = nullptr; // NUMBER
};

enum class NodeType : uint8
{
	NONE = 0,

	MODULE,

	// declarations
	CONST,
	STRUCT,
	FUNCTION,
	PARAMETER,
	FIELD,
	VARIABLE, // let: a local, or a global in the module
	ENUM_VALUE,
	TYPE_ALIAS,

	// statements
	BLOCK,
	RETURN,

	// expressions
	NUMBER,
	BOOL,
	IDENTIFIER,
	REFERENCE, // a resolved IDENTIFIER
	CONSTANT,  // a folded constant expression
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

// Children are found by usage, not position.
enum class Usage : uint8
{
	NONE = 0,
	ROOT,
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

enum class ElementKind : uint8
{
	NONE = 0,
	NODE,
	TYPE,
	INTRINSIC,
	CONSTANT,
	GENERIC,
};

enum class TypingState : uint8
{
	UNTYPED,
	TYPING,
	TYPED,
	FAILED,
};

struct Element
{
	ElementKind kind = ElementKind::NONE;
};

struct Node : Element
{
	NodeType node_type = NodeType::NONE;
	Usage usage = Usage::NONE;
	TypingState typing_state = TypingState::UNTYPED;
	Atom name = Atom::NONE;
	Type* type = nullptr;
	union
	{
		NumberLiteral* number = nullptr; // NUMBER
		bool value;						 // BOOL
		TokenType op;					 // UNARY, POSTFIX, BINARY
		Scope* scope;					 // MODULE, FUNCTION, BLOCK. A function shares its body's scope
		Element* target;				 // REFERENCE
		int64 enum_value;				 // ENUM_VALUE
		Constant* constant;				 // CONSTANT
	};
	Node* child = nullptr;
	Node* next = nullptr;

	Node()
	{
		kind = ElementKind::NODE;
	}
};

enum class OpKind : uint8
{
	PREFIX,
	INFIX,
	ARRAY,
};

struct PendingOp
{
	TokenType op = TokenType::NONE;
	OpKind kind = OpKind::INFIX;
	Node* payload = nullptr; // ARRAY: the size
};

struct ScriptError : Error
{
	ScriptError()
	{
		error_family = ErrorFamily::SCRIPT_ERROR;
	}
};

struct Parser
{
	char* source = nullptr;
	char* head = nullptr;
	Token token = {};
	Arena* arena = nullptr;
	Arena* error_arena = nullptr; // errors can outlive the AST
	std::vector<ScriptError*> errors;

	// Shared by nested expressions, each only touching entries above where it started.
	std::vector<Node*> operands;
	std::vector<PendingOp> operators;

	uint32 recursion_depth = 0;
};

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

struct GenericInstance;

// Types are unique, so they're compared by pointer.
struct Type : Element
{
	TypeKind type_kind = TypeKind::PRIMITIVE;
	Atom name = Atom::NONE;
	uint32 size = 0;
	uint32 alignment = 1;
	GenericInstance* instance = nullptr;

	Type()
	{
		kind = ElementKind::TYPE;
	}
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

	PrimitiveType()
	{
		type_kind = TypeKind::PRIMITIVE;
	}
};

struct VectorType : Type
{
	PrimitiveType* element = nullptr;
	uint32 count = 0;

	VectorType()
	{
		type_kind = TypeKind::VECTOR;
	}
};

struct MatrixType : Type
{
	PrimitiveType* element = nullptr;
	uint32 rows = 0;
	uint32 columns = 0;

	MatrixType()
	{
		type_kind = TypeKind::MATRIX;
	}
};

struct EnumType : Type
{
	Scope* scope = nullptr; // the ENUM_VALUEs

	EnumType()
	{
		type_kind = TypeKind::ENUM;
	}
};

struct ArrayType : Type
{
	Type* element = nullptr;
	uint32 length = 0;
	uint32 stride = 0;

	ArrayType()
	{
		type_kind = TypeKind::ARRAY;
	}
};

struct StructField
{
	Atom name = Atom::NONE;
	Type* type = nullptr;
	uint32 offset = 0;
	Node* declaration = nullptr;
};

enum class StructState : uint8
{
	DECLARED,
	COMPLETING,
	COMPLETE,
	FAILED,
};

// Declared by the resolver, laid out by the typer on first use.
struct StructType : Type
{
	Node* declaration = nullptr;
	Slice<StructField> fields;
	StructState state = StructState::DECLARED;

	StructType()
	{
		type_kind = TypeKind::STRUCT;
	}
};

// See Docs/IR.md.
enum class AddressSpace : uint8
{
	FUNCTION,
	PRIVATE,
	CONSTANT,
	INPUT,
	OUTPUT,
	MEMORY, // reserved
};

struct PointerType : Type
{
	AddressSpace space = AddressSpace::FUNCTION;
	Type* pointee = nullptr;

	PointerType()
	{
		type_kind = TypeKind::POINTER;
	}
};

struct FunctionType : Type
{
	Type* return_type = nullptr;
	Slice<Type*> parameters;

	FunctionType()
	{
		type_kind = TypeKind::FUNCTION;
	}
};

enum class Semantic : uint8
{
	VERTEX_INDEX,
	POSITION,
};

enum class IntrinsicKind : uint8
{
	NONE,

	// attributes
	SEMANTIC,
	LOCATION,
	ENTRY,

	// built-in functions, last
	MUL,
	MIN,
	MAX,
	DOT,
	LENGTH,
	NORMALIZE,
};

struct Intrinsic : Element
{
	IntrinsicKind intrinsic_kind = IntrinsicKind::NONE;
	Atom name = Atom::NONE;
	Scope* argument_scope = nullptr; // where a call's arguments resolve, nullptr for the call's own scope

	Intrinsic()
	{
		kind = ElementKind::INTRINSIC;
	}
};

// Laid out by the type, padding zeroed.
struct Constant : Element
{
	Type* type = nullptr;
	Slice<uint8> bytes;

	Constant()
	{
		kind = ElementKind::CONSTANT;
	}
};

enum class GenericParamKind : uint8
{
	TYPE,
	CONSTANT,
};

struct GenericArg
{
	Type* type = nullptr;
	Constant* constant = nullptr;
};

struct GenericParam
{
	GenericParamKind kind = GenericParamKind::TYPE;
	const char* what = "";
	PrimitiveType* type = nullptr; // CONSTANT: what the argument converts to
};

struct GenericInstance
{
	Generic* generic = nullptr;
	Slice<GenericArg> args;
};

// The arguments are already typed and converted. Returns nullptr with errors through typer if it rejects them; typer is
// nullptr for internal callers. The instance cache is in front of it, so it's never called twice for the same arguments.
typedef Type* (*InstantiateFn)(Context& context, GenericInstance* instance, Typer* typer);

struct Generic : Element
{
	Atom name = Atom::NONE;
	Slice<GenericParam> params;
	InstantiateFn instantiate = nullptr;

	Generic()
	{
		kind = ElementKind::GENERIC;
	}
};

struct GenericInstanceKey
{
	const GenericInstance* instance = nullptr;

	bool operator==(const GenericInstanceKey& other) const;
};

struct GenericInstanceHash
{
	size_t operator()(const GenericInstanceKey& key) const;
};

struct Definition
{
	Atom name = Atom::NONE;
	Element* element = nullptr;
	Definition* next = nullptr;
};

enum class ScopeKind : uint8
{
	GLOBAL,
	ENUM,
	ARGUMENTS,
	MODULE,
	FUNCTION,
	BLOCK,
};

struct Scope
{
	ScopeKind kind = ScopeKind::GLOBAL;
	Scope* parent = nullptr;
	Definition* first = nullptr;
};

enum class ContextKind : uint8
{
	SCRIPT,
	SHADER,
};

struct Context
{
	Arena* arena = nullptr;
	Scope* global_scope = nullptr;

	PrimitiveType* void_type = nullptr;
	PrimitiveType* bool_type = nullptr; // not in the global scope yet
	PrimitiveType* int_type = nullptr;
	PrimitiveType* uint_type = nullptr;
	PrimitiveType* float_type = nullptr;
	EnumType* semantic_type = nullptr; // SHADER only
	EnumType* stage_type = nullptr;	   // SHADER only
	Generic* array_generic = nullptr;
	Generic* vector_generic = nullptr;
	Generic* matrix_generic = nullptr;

	// Instances can refer to a module's types, so modules have to live as long as their context (TODO.md).
	std::unordered_map<GenericInstanceKey, Type*, GenericInstanceHash> instances;
	std::vector<PointerType*> pointer_types;
	std::vector<FunctionType*> function_types;
};

struct Resolver
{
	Context* context = nullptr;
	Arena* arena = nullptr;
	Arena* error_arena = nullptr;
	std::vector<ScriptError*> errors;
	Scope* scope = nullptr;
	uint32 recursion_depth = 0;
};

struct Typer
{
	Context* context = nullptr;
	Arena* arena = nullptr;
	Arena* error_arena = nullptr;
	std::vector<ScriptError*> errors;
	Type* return_type = nullptr; // of the function being typed
	uint64 constant_size = 0;
	uint32 recursion_depth = 0;
};

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

struct ShaderIO
{
	IODirection direction = IODirection::INPUT;
	IOKind io_kind = IOKind::SEMANTIC;
	Semantic semantic = Semantic::VERTEX_INDEX; // SEMANTIC
	uint32 location = 0;						// LOCATION
	Type* type = nullptr;
	Slice<uint32> path; // INPUT: [parameter, field, ...]. OUTPUT: [field, ...]
	Node* declaration = nullptr;
};

struct EntryPoint
{
	ShaderStage stage = ShaderStage::VERTEX;
	Node* function = nullptr;
	Slice<ShaderIO> io;
};

struct ShaderInterface
{
	Slice<EntryPoint> entry_points;
};

struct ShaderInterfaceBuilder
{
	Arena* arena = nullptr;
	Arena* error_arena = nullptr;
	std::vector<ScriptError*> errors;
	uint32 recursion_depth = 0;
};

struct CompileShaderOptions
{
	Arena* arena = nullptr; // for the result
	ZTStringView source;
	GPU::Backend backend = GPU::Backend::NONE; // VULKAN: SPIR-V 1.0. D3D11: HLSL for fxc, SM 5.0. METAL: MSL 2.0
};

struct CompileShaderResult
{
	Slice<GPU::CompiledEntryPoint> entry_points; // in source order
	Slice<ScriptError*> errors;
};

extern uint32 RECURSION_LIMIT;
extern uint32 CONSTANT_SIZE_LIMIT;
extern uint64 TOTAL_CONSTANT_SIZE_LIMIT;

inline Node* FindChild(Node* node, Usage usage)
{
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->usage == usage)
			return child;
	}
	return nullptr;
}

inline bool IsBuiltinFunction(IntrinsicKind kind)
{
	return kind >= IntrinsicKind::MUL;
}

// The built-in types, generics and intrinsics, in the global scope above every module. arena has to outlive the
// context. Types are unique: generic instances, pointer and function types are cached here, and the Get functions only
// take valid arguments.
void InitContext(Context& context, Arena* arena, ContextKind kind);

Type* Instantiate(Context& context, Generic* generic, Slice<GenericArg> args, Typer* typer);
ArrayType* GetArrayType(Context& context, Type* element, uint32 length);
VectorType* GetVectorType(Context& context, PrimitiveType* element, uint32 count);
MatrixType* GetMatrixType(Context& context, PrimitiveType* element, uint32 columns, uint32 rows);
PointerType* GetPointerType(Context& context, AddressSpace space, Type* pointee);
FunctionType* GetFunctionType(Context& context, Type* return_type, Slice<Type*> parameters);

// --- LEXER & PARSER -----------------------------------------

// Parses a source file into a MODULE whose children are its declarations. Stops at the first error.
bool Parse(Parser& parser, Node** out_module);

Node* ParseExpression(Parser& parser);
Node* ParseStatement(Parser& parser);
bool LexToken(Parser& parser);
void EatToken(Parser& parser);
ScriptError* EmitError(Parser& parser, const char* format, ...);

// --- RESOLVER -----------------------------------------------

// Turns IDENTIFIERs into REFERENCEs and gives MODULE, FUNCTION and BLOCK nodes their scope. Functions, structs, type
// aliases and globals can be referenced anywhere in their scope, everything else only after its declaration. Unknown
// names stay IDENTIFIERs.
bool Resolve(Resolver& resolver, Node* module);

ScriptError* EmitError(Resolver& resolver, const char* format, ...);

// --- TYPER --------------------------------------------------

// Gives every expression and declaration its type. Constant expressions (a const's value, array sizes, locations)
// aren't typed but evaluated, and become CONSTANT nodes. Errors don't stop typing the rest of the module.
bool TypeCheck(Typer& typer, Node* module);
ScriptError* EmitError(Typer& typer, const char* format, ...);
bool TypeConst(Typer& typer, Node* node);
Type* EvaluateType(Typer& typer, Node* node);
bool ResolveName(Typer& typer, Node* node, Element** out);
Constant* EvaluateConstant(Typer& typer, Node* node, Type* expected, const char* what);
Constant* TryEvaluateConstant(Typer& typer, Node* node, Type* expected);
int64 ConstantToInteger(Constant* constant);
PrimitiveType* NumberType(Typer& typer, NumberLiteral* number, Type* expected);
bool CheckOperator(Typer& typer, NodeType node_type, TokenType op);
bool CheckOperands(Typer& typer, NodeType node_type, TokenType op, Type* type);
bool AddConstructorArgument(Typer& typer, VectorType* vector, Type* type, uint32* components);
bool CheckConstructorComponents(Typer& typer, VectorType* vector, uint32 components);
PrimitiveType* ComponentType(Type* type);
uint32 ComponentCount(Type* type);
bool IsNumeric(PrimitiveType* type);
bool IsInteger(PrimitiveType* type);

// --- SHADER INTERFACE PASS ----------------------------------

// Finds the entry points of a typed shader module and flattens their parameters and return values into inputs and
// outputs, checking their semantics and locations.
bool BuildShaderInterface(ShaderInterfaceBuilder& builder, Node* module, ShaderInterface* out_interface);
ScriptError* EmitError(ShaderInterfaceBuilder& builder, const char* format, ...);

// --- PRINTING -----------------------------------------------

ZTStringView TokenToString(TokenType token_type);
ZTStringView NodeTypeToString(NodeType type);
ZTStringView UsageToString(Usage usage);
ZTStringView AddressSpaceToString(AddressSpace space);
ZTStringView SemanticToString(Semantic semantic);
ZTStringView ShaderStageToString(ShaderStage stage);
ZTStringView TypeToString(Type* type, Arena* arena);
ZTStringView NumberToString(NumberLiteral* number, Arena* arena);
ZTStringView ConstantToString(Constant* constant, Arena* arena);
ZTStringView ShaderInterfaceToString(ShaderInterface& shader_interface, Arena* arena);
void SnapshotNodeToString(StringBuilder& builder, Node* node);
void DumpNode(Node* node, Arena* arena, int indent = 0);

// ------------------------------------------------------------

// Runs every stage on a shader's source. Errors in the source come from the front end; past it, only the target's size
// limits can fail, and fxc can still run out of registers.
CompileShaderResult CompileShader(const CompileShaderOptions& options);

}
