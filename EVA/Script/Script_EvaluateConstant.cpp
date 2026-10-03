#include <EVA/Script/Script.hpp>
#include <math.h>
#include <string.h>
#include <algorithm>

// Constant expressions are evaluated in two passes over the same subtree, neither typing its nodes: ConstantType works
// out the type, so the result can be allocated, and EvaluateInto writes the value straight into its place in it. Only
// the result is allocated; operands of operators and indices are evaluated on the stack.

namespace EVA::Script
{

uint32 CONSTANT_SIZE_LIMIT = 4 * 1024 * 1024;
uint64 TOTAL_CONSTANT_SIZE_LIMIT = 16ull * 1024 * 1024;

// Operands of operators, at most a vector for now. Room for a 4x4 float matrix.
static const uint32 MAX_OPERAND_SIZE = 64;

struct Evaluation
{
	Typer& typer;
	bool not_constant = false;   // found something that isn't a constant expression, without an error
	bool failed_before = false;  // refers to a const that failed, already reported
};

int64 ConstantToInteger(Constant* constant)
{
	uint32 bits = ReadComponent(constant->bytes, 0);
	return ((PrimitiveType*)constant->type)->primitive_kind == PrimitiveKind::SIGNED ? (int64)(int32)bits : (int64)bits;
}

// The literal as type, which it fits.
static uint32 GetNumberBits(NumberLiteral* number, PrimitiveType* type)
{
	if (type->primitive_kind != PrimitiveKind::FLOAT)
		return (uint32)number->integer;
	return FloatToBits(number->kind == NumberKind::INTEGER ? (float)number->integer : number->f32);
}

static uint32 FoldUnary(PrimitiveKind kind, TokenType op, uint32 a)
{
	switch (op)
	{
	case TokenType::MINUS: return kind == PrimitiveKind::FLOAT ? FloatToBits(-BitsToFloat(a)) : 0u - a;
	case TokenType::TILDE: return ~a;
	default: return a;
	}
}

// + - * on integers wrap. Division by zero and INT_MIN / -1 are errors.
static bool FoldBinary(Typer& typer, PrimitiveKind kind, TokenType op, uint32 a, uint32 b, uint32* out)
{
	if (kind == PrimitiveKind::FLOAT)
	{
		float x = BitsToFloat(a);
		float y = BitsToFloat(b);
		float result = 0.0f;
		switch (op)
		{
		case TokenType::PLUS: result = x + y; break;
		case TokenType::MINUS: result = x - y; break;
		case TokenType::ASTERISK: result = x * y; break;
		case TokenType::SLASH: result = x / y; break;
		case TokenType::PERCENT: result = fmodf(x, y); break;
		default: return false;
		}
		*out = FloatToBits(result);
		return true;
	}

	switch (op)
	{
	case TokenType::PLUS: *out = a + b; return true;
	case TokenType::MINUS: *out = a - b; return true;
	case TokenType::ASTERISK: *out = a * b; return true;
	default: break;
	}
	if (b == 0)
	{
		EmitError(typer.context, "division by zero");
		return false;
	}
	if (kind == PrimitiveKind::SIGNED)
	{
		int32 x = (int32)a;
		int32 y = (int32)b;
		if (x == INT32_MIN && y == -1)
		{
			EmitError(typer.context, "integer overflow");
			return false;
		}
		*out = (uint32)(op == TokenType::SLASH ? x / y : x % y);
		return true;
	}
	*out = op == TokenType::SLASH ? a / b : a % b;
	return true;
}

static Type* GetConstantType(Evaluation& evaluation, Node* node, Type* expected);

static Type* GetReferenceType(Evaluation& evaluation, Node* node)
{
	Typer& typer = evaluation.typer;
	switch (node->target->kind)
	{
	case ElementKind::NODE:
	{
		Node* target = (Node*)node->target;
		if (target->node_type == NodeType::TYPE_ALIAS)
		{
			EmitError(typer.context, "'%s' is a type, not a value", GetAtomNameCString(typer.context, node->name));
			return nullptr;
		}
		if (target->node_type != NodeType::CONST)
			break;
		if (!TypeConst(typer, target))
		{
			evaluation.failed_before = true;
			return nullptr;
		}
		return target->type;
	}
	case ElementKind::CONSTANT: return ((Constant*)node->target)->type;
	case ElementKind::TYPE:
	case ElementKind::GENERIC:
		EmitError(typer.context, "'%s' is a type, not a value", GetAtomNameCString(typer.context, node->name));
		return nullptr;
	case ElementKind::INTRINSIC:
		if (IsBuiltinFunction(((Intrinsic*)node->target)->intrinsic_kind))
			EmitError(typer.context, "'%s' is a function, which can only be called", GetAtomNameCString(typer.context, node->name));
		else
			EmitError(typer.context, "'%s' can only be used as an attribute", GetAtomNameCString(typer.context, node->name));
		return nullptr;
	case ElementKind::NONE: break;
	}
	evaluation.not_constant = true; // variables, parameters, functions
	return nullptr;
}

// The operands' types of a BINARY with the typer's rules: a literal takes the other side's type, and two literals are
// float if either is written as one.
static bool GetOperandTypes(Evaluation& evaluation, Node* node, Type* expected, Type** out_left, Type** out_right)
{
	Node* left = FindChild(node, Usage::LEFT);
	Node* right = FindChild(node, Usage::RIGHT);
	bool left_literal = left->node_type == NodeType::NUMBER;
	bool right_literal = right->node_type == NodeType::NUMBER;
	if (left_literal && right_literal && !expected &&
		(left->number->kind == NumberKind::FLOAT || right->number->kind == NumberKind::FLOAT))
		expected = evaluation.typer.context.float_type;

	bool swap = left_literal && !right_literal;
	Type* first = GetConstantType(evaluation, swap ? right : left, expected);
	if (!first)
		return false;
	Type* second = GetConstantType(evaluation, swap ? left : right, first);
	if (!second)
		return false;
	*out_left = swap ? second : first;
	*out_right = swap ? first : second;
	if (*out_left != *out_right)
	{
		Typer& typer = evaluation.typer;
		EmitError(typer.context, "mismatched types %s and %s", GetTypeNameCString(typer.context, *out_left), GetTypeNameCString(typer.context, *out_right));
		return false;
	}
	return true;
}

static Type* GetConstructorType(Evaluation& evaluation, Node* node)
{
	Typer& typer = evaluation.typer;
	Node* callee = FindChild(node, Usage::CALLEE);
	Element* target = nullptr;
	if (!ResolveName(typer, callee, &target))
	{
		evaluation.failed_before = true; // reported
		return nullptr;
	}
	if (target && target->kind == ElementKind::GENERIC)
	{
		EmitError(typer.context, "expected a value, got a type"); // the call instantiates the generic
		return nullptr;
	}
	if (!target || target->kind != ElementKind::TYPE)
	{
		evaluation.not_constant = true; // calls to functions
		return nullptr;
	}
	Type* type = (Type*)target;
	if (type->type_kind != TypeKind::VECTOR)
	{
		EmitError(typer.context, "constructing %s isn't supported yet", GetTypeNameCString(typer.context, type));
		return nullptr;
	}

	VectorType* vector = (VectorType*)type;
	uint32 components = 0;
	for (Node* argument = node->child; argument; argument = argument->next)
	{
		if (argument->usage != Usage::ARGUMENT)
			continue;
		Type* argument_type = GetConstantType(evaluation, argument, vector->element);
		if (!argument_type || !AddConstructorArgument(typer, vector, argument_type, &components))
			return nullptr;
	}
	if (!CheckConstructorComponents(typer, vector, components))
		return nullptr;
	return type;
}

static uint32 GetElementCount(Node* node)
{
	uint32 count = 0;
	for (Node* element = node->child; element; element = element->next)
		count += element->usage == Usage::ELEMENT;
	return count;
}

// The type of the index'th element of an array or struct type.
static Type* GetElementType(Type* type, uint32 index)
{
	if (type->type_kind == TypeKind::ARRAY)
		return ((ArrayType*)type)->element;
	return ((StructType*)type)->fields[index].type;
}

static uint32 GetElementOffset(Type* type, uint32 index)
{
	if (type->type_kind == TypeKind::ARRAY)
		return index * ((ArrayType*)type)->stride;
	return ((StructType*)type)->fields[index].offset;
}

static Type* GetInitListType(Evaluation& evaluation, Node* node, Type* expected)
{
	Typer& typer = evaluation.typer;
	if (!expected)
	{
		EmitError(typer.context, "can't tell the type of an initializer list here");
		return nullptr;
	}
	uint32 expected_count = 0;
	if (expected->type_kind == TypeKind::ARRAY)
		expected_count = ((ArrayType*)expected)->length;
	else if (expected->type_kind == TypeKind::STRUCT)
		expected_count = ((StructType*)expected)->fields.count;
	else
	{
		EmitError(typer.context, "can't initialize %s with an initializer list", GetTypeNameCString(typer.context, expected));
		return nullptr;
	}
	uint32 count = GetElementCount(node);
	if (count != expected_count)
	{
		EmitError(typer.context, "%s needs %u elements, got %u", GetTypeNameCString(typer.context, expected), expected_count, count);
		return nullptr;
	}

	uint32 index = 0;
	for (Node* element = node->child; element; element = element->next)
	{
		if (element->usage != Usage::ELEMENT)
			continue;
		Type* element_type = GetElementType(expected, index++);
		Type* type = GetConstantType(evaluation, element, element_type);
		if (!type)
			return nullptr;
		if (type != element_type)
		{
			EmitError(typer.context, "expected %s, got %s", GetTypeNameCString(typer.context, element_type), GetTypeNameCString(typer.context, type));
			return nullptr;
		}
	}
	return expected;
}

static Type* GetIndexType(Evaluation& evaluation, Node* node)
{
	Typer& typer = evaluation.typer;
	Type* object = GetConstantType(evaluation, FindChild(node, Usage::OBJECT), nullptr);
	if (!object)
		return nullptr;
	if (object->type_kind != TypeKind::ARRAY)
	{
		EmitError(typer.context, "can't index %s", GetTypeNameCString(typer.context, object));
		return nullptr;
	}
	Type* index = GetConstantType(evaluation, FindChild(node, Usage::INDEX), typer.context.uint_type);
	if (!index)
		return nullptr;
	PrimitiveType* index_type = GetComponentType(index);
	if (!index_type || GetComponentCount(index) != 1 || !IsInteger(index_type))
	{
		EmitError(typer.context, "index must be an int or uint, got %s", GetTypeNameCString(typer.context, index));
		return nullptr;
	}
	return ((ArrayType*)object)->element;
}

// The field of a struct type, nullptr with an error if there's none called name.
static StructField* FindField(Typer& typer, Type* type, Atom name)
{
	if (type->type_kind == TypeKind::STRUCT)
	{
		StructType* structure = (StructType*)type;
		for (uint32 i = 0; i < structure->fields.count; ++i)
		{
			if (structure->fields[i].name == name)
				return &structure->fields[i];
		}
	}
	EmitError(typer.context, "%s has no member '%s'", GetTypeNameCString(typer.context, type), GetAtomNameCString(typer.context, name));
	return nullptr;
}

// The type of a constant expression, or nullptr: with an error, or with not_constant set if it isn't one.
static Type* GetConstantType(Evaluation& evaluation, Node* node, Type* expected)
{
	Typer& typer = evaluation.typer;
	CHECK_RECURSION(typer.context);
	switch (node->node_type)
	{
	case NodeType::NUMBER: return GetNumberType(typer, node->number, expected);
	case NodeType::CONSTANT: return node->type;
	case NodeType::REFERENCE: return GetReferenceType(evaluation, node);
	case NodeType::UNARY:
	{
		if (!CheckOperator(typer, NodeType::UNARY, node->op))
			return nullptr;
		Type* type = GetConstantType(evaluation, FindChild(node, Usage::OPERAND), expected);
		if (!type || !CheckOperands(typer, NodeType::UNARY, node->op, type))
			return nullptr;
		return type;
	}
	case NodeType::BINARY:
	{
		if (!CheckOperator(typer, NodeType::BINARY, node->op))
			return nullptr;
		Type* left = nullptr;
		Type* right = nullptr;
		if (!GetOperandTypes(evaluation, node, expected, &left, &right) || !CheckOperands(typer, NodeType::BINARY, node->op, left))
			return nullptr;
		return left;
	}
	case NodeType::CALL: return GetConstructorType(evaluation, node);
	case NodeType::INIT_LIST: return GetInitListType(evaluation, node, expected);
	case NodeType::INDEX: return GetIndexType(evaluation, node);
	case NodeType::MEMBER:
	{
		Type* object = GetConstantType(evaluation, FindChild(node, Usage::OBJECT), nullptr);
		if (!object)
			return nullptr;
		StructField* field = FindField(typer, object, node->name);
		return field ? field->type : nullptr;
	}
	case NodeType::ARRAY_TYPE:
		EmitError(typer.context, "expected a value, got a type");
		return nullptr;
	default:
		evaluation.not_constant = true; // bools, ifs, assignments...
		return nullptr;
	}
}

static bool EvaluateInto(Evaluation& evaluation, Node* node, Type* type, Slice<uint8> out);

// Where an INDEX or MEMBER's object, or a reference, already has its value: inside a const's constant. Objects can only
// be those, since an initializer list needs an expected type and indexing and member access don't give one.
static const uint8* GetConstantPlace(Evaluation& evaluation, Node* node, Type* type)
{
	Typer& typer = evaluation.typer;
	CHECK_RECURSION(typer.context);
	switch (node->node_type)
	{
	case NodeType::CONSTANT: return node->constant->bytes.data;
	case NodeType::REFERENCE:
		if (node->target->kind == ElementKind::CONSTANT)
			return ((Constant*)node->target)->bytes.data;
		return FindChild((Node*)node->target, Usage::VALUE)->constant->bytes.data; // a CONST's folded value
	case NodeType::INDEX:
	{
		Node* object = FindChild(node, Usage::OBJECT);
		Node* index = FindChild(node, Usage::INDEX);
		ArrayType* array = (ArrayType*)GetConstantType(evaluation, object, nullptr);
		Type* index_type = GetConstantType(evaluation, index, typer.context.uint_type);
		const uint8* place = GetConstantPlace(evaluation, object, array);
		uint8 bits[4];
		if (!place || !EvaluateInto(evaluation, index, index_type, Slice<uint8>(bits, 4)))
			return nullptr;
		uint32 value_bits = ReadComponent(Slice<uint8>(bits, 4), 0);
		int64 value = ((PrimitiveType*)index_type)->primitive_kind == PrimitiveKind::SIGNED ? (int64)(int32)value_bits : (int64)value_bits;
		if (value < 0 || value >= array->length)
		{
			EmitError(typer.context, "index %lld is out of bounds for %s", (long long)value, GetTypeNameCString(typer.context, array));
			return nullptr;
		}
		return place + (size_t)value * array->stride;
	}
	case NodeType::MEMBER:
	{
		Node* object = FindChild(node, Usage::OBJECT);
		Type* object_type = GetConstantType(evaluation, object, nullptr);
		const uint8* place = GetConstantPlace(evaluation, object, object_type);
		return place ? place + FindField(typer, object_type, node->name)->offset : nullptr;
	}
	default: break;
	}
	(void)type;
	assert(!"not a place");
	return nullptr;
}

// Writes node's value as type, what ConstantType gave it, to out, which is type's size.
static bool EvaluateInto(Evaluation& evaluation, Node* node, Type* type, Slice<uint8> out)
{
	Typer& typer = evaluation.typer;
	CHECK_RECURSION(typer.context);
	assert(out.count == type->size);
	switch (node->node_type)
	{
	case NodeType::NUMBER: WriteComponent(out, 0, GetNumberBits(node->number, (PrimitiveType*)type)); return true;
	case NodeType::CONSTANT:
	case NodeType::REFERENCE:
	case NodeType::INDEX:
	case NodeType::MEMBER:
	{
		const uint8* place = GetConstantPlace(evaluation, node, type);
		if (!place)
			return false;
		memcpy(out.data, place, type->size);
		return true;
	}
	case NodeType::UNARY:
	{
		// The operand has the result's type.
		assert(type->size <= MAX_OPERAND_SIZE);
		uint8 operand[MAX_OPERAND_SIZE];
		Slice<uint8> operand_bytes(operand, type->size);
		if (!EvaluateInto(evaluation, FindChild(node, Usage::OPERAND), type, operand_bytes))
			return false;
		PrimitiveKind kind = GetComponentType(type)->primitive_kind;
		for (uint32 i = 0; i < GetComponentCount(type); ++i)
			WriteComponent(out, i, FoldUnary(kind, node->op, ReadComponent(operand_bytes, i)));
		return true;
	}
	case NodeType::BINARY:
	{
		// Both operands have the result's type.
		assert(type->size <= MAX_OPERAND_SIZE);
		uint8 left[MAX_OPERAND_SIZE];
		uint8 right[MAX_OPERAND_SIZE];
		Slice<uint8> left_bytes(left, type->size);
		Slice<uint8> right_bytes(right, type->size);
		if (!EvaluateInto(evaluation, FindChild(node, Usage::LEFT), type, left_bytes) ||
			!EvaluateInto(evaluation, FindChild(node, Usage::RIGHT), type, right_bytes))
			return false;
		PrimitiveKind kind = GetComponentType(type)->primitive_kind;
		for (uint32 i = 0; i < GetComponentCount(type); ++i)
		{
			uint32 bits = 0;
			if (!FoldBinary(typer, kind, node->op, ReadComponent(left_bytes, i), ReadComponent(right_bytes, i), &bits))
				return false;
			WriteComponent(out, i, bits);
		}
		return true;
	}
	case NodeType::CALL:
	{
		// A vector constructor: the arguments' components in order, or one scalar for all of them.
		VectorType* vector = (VectorType*)type;
		uint32 offset = 0;
		for (Node* argument = node->child; argument; argument = argument->next)
		{
			if (argument->usage != Usage::ARGUMENT)
				continue;
			Type* argument_type = GetConstantType(evaluation, argument, vector->element);
			assert((uint64)offset + argument_type->size <= out.count);
			if (!EvaluateInto(evaluation, argument, argument_type, Slice<uint8>(out.data + offset, argument_type->size)))
				return false;
			offset += argument_type->size;
		}
		if (offset == 4)
		{
			for (uint32 i = 1; i < vector->count; ++i)
				WriteComponent(out, i, ReadComponent(out, 0));
		}
		return true;
	}
	case NodeType::INIT_LIST:
	{
		uint32 index = 0;
		for (Node* element = node->child; element; element = element->next)
		{
			if (element->usage != Usage::ELEMENT)
				continue;
			Type* element_type = GetElementType(type, index);
			uint32 offset = GetElementOffset(type, index);
			index++;
			assert((uint64)offset + element_type->size <= out.count);
			if (!EvaluateInto(evaluation, element, element_type, Slice<uint8>(out.data + offset, element_type->size)))
				return false;
		}
		return true;
	}
	default: break;
	}
	assert(!"ConstantType accepted a node EvaluateInto doesn't handle");
	return false;
}

// nullptr with an error past the limits.
static Constant* NewConstant(Typer& typer, Type* type)
{
	if (type->size > CONSTANT_SIZE_LIMIT)
	{
		EmitError(typer.context, "%s is too large for a constant: %u bytes, at most %u", GetTypeNameCString(typer.context, type), type->size,
			CONSTANT_SIZE_LIMIT);
		return nullptr;
	}
	if (typer.constant_size + type->size > TOTAL_CONSTANT_SIZE_LIMIT)
	{
		EmitError(typer.context, "constants take more than %llu bytes in total", (unsigned long long)TOTAL_CONSTANT_SIZE_LIMIT);
		return nullptr;
	}
	typer.constant_size += type->size;

	Constant* constant = typer.arena->New<Constant>();
	constant->type = type;
	uint8* bytes = (uint8*)typer.arena->Allocate(type->size, type->alignment);
	memset(bytes, 0, type->size); // padding
	constant->bytes = Slice<uint8>(bytes, type->size);
	return constant;
}

// Replaces node with a CONSTANT of its value. The attributes of node and of everything under it move to the CONSTANT,
// ignored for now, so none get lost.
static void Fold(Node* node, Constant* constant)
{
	Node* attributes = nullptr;
	Node** tail = &attributes;
	std::vector<Node*> stack = { node }; // the tree can be as deep as the recursion limit
	while (!stack.empty())
	{
		Node* current = stack.back();
		stack.pop_back();
		size_t first = stack.size();
		for (Node* child = current->child; child; child = child->next)
		{
			// Linking an attribute only changes the next of the previous one, which the loop is already past.
			if (child->usage == Usage::ATTRIBUTE)
			{
				*tail = child;
				tail = &child->next;
			}
			else
				stack.push_back(child);
		}
		std::reverse(stack.begin() + (ptrdiff_t)first, stack.end()); // source order
	}
	*tail = nullptr;
	node->node_type = NodeType::CONSTANT;
	node->name = Atom::NONE;
	node->type = constant->type;
	node->constant = constant;
	node->child = attributes;
}

// what: for the error if node isn't a constant expression, nullptr for none.
static Constant* Evaluate(Typer& typer, Node* node, Type* expected, const char* what)
{
	Evaluation evaluation = { .typer = typer };
	size_t error_count = typer.context.errors.size();
	Constant* constant = nullptr;
	if (Type* type = GetConstantType(evaluation, node, expected))
	{
		constant = NewConstant(typer, type);
		if (constant && !EvaluateInto(evaluation, node, type, constant->bytes))
			constant = nullptr;
	}
	assert(constant || evaluation.not_constant || evaluation.failed_before || typer.context.errors.size() > error_count);
	if (!constant && what && evaluation.not_constant && typer.context.errors.size() == error_count)
		EmitError(typer.context, "%s must be a constant", what);
	if (constant)
		Fold(node, constant);
	return constant;
}

Constant* EvaluateConstant(Typer& typer, Node* node, Type* expected, const char* what)
{
	return Evaluate(typer, node, expected, what);
}

Constant* TryEvaluateConstant(Typer& typer, Node* node, Type* expected)
{
	return Evaluate(typer, node, expected, nullptr);
}

}
