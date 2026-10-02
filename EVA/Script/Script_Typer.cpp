#include <EVA/Script/Script.hpp>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

namespace EVA::Script
{

ScriptError* EmitError(Typer& typer, const char* format, ...)
{
	ScriptError* error = typer.error_arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(typer.error_arena, format, args);
	va_end(args);
	typer.errors.push_back(error);
	return error;
}

static const char* AtomName(Typer& typer, Atom atom)
{
	return GetAtomString(atom, typer.arena).CString();
}

static const char* TypeName(Typer& typer, Type* type)
{
	return TypeToString(type, typer.arena).CString();
}

// The scalar of a primitive or vector type, nullptr for anything else.
static PrimitiveType* ComponentType(Type* type)
{
	if (type->type_kind == TypeKind::PRIMITIVE)
		return (PrimitiveType*)type;
	if (type->type_kind == TypeKind::VECTOR)
		return ((VectorType*)type)->element;
	return nullptr;
}

static uint32 ComponentCount(Type* type)
{
	return type->type_kind == TypeKind::VECTOR ? ((VectorType*)type)->count : 1;
}

static bool IsNumeric(PrimitiveType* type)
{
	return type->primitive_kind == PrimitiveKind::SIGNED || type->primitive_kind == PrimitiveKind::UNSIGNED ||
		   type->primitive_kind == PrimitiveKind::FLOAT;
}

static bool IsInteger(PrimitiveType* type)
{
	return type->primitive_kind == PrimitiveKind::SIGNED || type->primitive_kind == PrimitiveKind::UNSIGNED;
}

// Returns node as target_type, or nullptr without an error if it can't be.
static Node* TryImplicitCast(Typer& typer, Node* node, Type* target_type)
{
	(void)typer;
	if (node->type == target_type)
		return node;
	// Lossless conversions go here.
	return nullptr;
}

static Node* ImplicitCast(Typer& typer, Node* node, Type* target_type)
{
	if (Node* cast = TryImplicitCast(typer, node, target_type))
		return cast;
	EmitError(typer, "expected %s, got %s", TypeName(typer, target_type), TypeName(typer, node->type));
	return nullptr;
}

// Casts a to b's type, or else b to a's.
static bool ImplicitCoCast(Typer& typer, Node*& a, Node*& b)
{
	if (Node* cast = TryImplicitCast(typer, a, b->type))
	{
		a = cast;
		return true;
	}
	if (Node* cast = TryImplicitCast(typer, b, a->type))
	{
		b = cast;
		return true;
	}
	EmitError(typer, "mismatched types %s and %s", TypeName(typer, a->type), TypeName(typer, b->type));
	return false;
}

static bool LooksLikeFloat(const char* text)
{
	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		return false;
	return strchr(text, '.') || strchr(text, 'e') || strchr(text, 'E');
}

// digits [. digits] [e [+-] digits], with at least one digit before the exponent.
static bool IsDecimalFloat(const char* text)
{
	uint32 digits = 0;
	while (*text >= '0' && *text <= '9')
	{
		text++;
		digits++;
	}
	if (*text == '.')
	{
		text++;
		while (*text >= '0' && *text <= '9')
		{
			text++;
			digits++;
		}
	}
	if (!digits)
		return false;
	if (*text == 'e' || *text == 'E')
	{
		text++;
		if (*text == '+' || *text == '-')
			text++;
		if (!(*text >= '0' && *text <= '9'))
			return false;
		while (*text >= '0' && *text <= '9')
			text++;
	}
	return !*text;
}

// The NUMBER's text as type, which is int, uint or float. Integers are decimal or 0x hex, floats decimal.
static bool ParseNumber(Typer& typer, Node* node, PrimitiveType* type, uint32* out_bits)
{
	const char* text = node->text;
	if (type->primitive_kind == PrimitiveKind::FLOAT)
	{
		if (!IsDecimalFloat(text))
		{
			EmitError(typer, "'%s' is not a valid float", text);
			return false;
		}
		float value = strtof(text, nullptr);
		if (isinf(value))
		{
			EmitError(typer, "'%s' is out of range for float", text);
			return false;
		}
		memcpy(out_bits, &value, sizeof(value));
		return true;
	}

	bool hex = text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
	const char* digit = hex ? text + 2 : text;
	uint32 base = hex ? 16 : 10;
	uint64 limit = type->primitive_kind == PrimitiveKind::SIGNED ? INT32_MAX : UINT32_MAX;
	uint64 value = 0;
	if (!*digit)
	{
		EmitError(typer, "'%s' is not a valid %s", text, TypeName(typer, type));
		return false;
	}
	for (; *digit; ++digit)
	{
		char ch = *digit;
		uint32 digit_value = 16;
		if (ch >= '0' && ch <= '9')
			digit_value = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			digit_value = ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			digit_value = ch - 'A' + 10;
		if (digit_value >= base)
		{
			EmitError(typer, "'%s' is not a valid %s", text, TypeName(typer, type));
			return false;
		}
		value = value * base + digit_value;
		if (value > limit)
		{
			EmitError(typer, "'%s' is out of range for %s", text, TypeName(typer, type));
			return false;
		}
	}
	*out_bits = (uint32)value;
	return true;
}

static Constant* NewConstant(Typer& typer, Type* type)
{
	Constant* constant = typer.arena->New<Constant>();
	constant->type = type;
	uint8* bytes = (uint8*)typer.arena->Allocate(type->size, type->alignment);
	memset(bytes, 0, type->size);
	constant->bytes = Slice<uint8>(bytes, type->size);
	return constant;
}

static uint32 ReadComponent(Constant* constant, uint32 index)
{
	uint32 bits;
	memcpy(&bits, constant->bytes.data + index * 4, 4);
	return bits;
}

static void WriteComponent(Constant* constant, uint32 index, uint32 bits)
{
	memcpy(constant->bytes.data + index * 4, &bits, 4);
}

// A constant scalar int or uint as int64.
static int64 ConstantToInteger(Constant* constant)
{
	uint32 bits = ReadComponent(constant, 0);
	return ((PrimitiveType*)constant->type)->primitive_kind == PrimitiveKind::SIGNED ? (int64)(int32)bits : (int64)bits;
}

static float BitsToFloat(uint32 bits)
{
	float value;
	memcpy(&value, &bits, 4);
	return value;
}

static uint32 FloatToBits(float value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	return bits;
}

static uint32 FoldUnary(PrimitiveKind kind, TokenType op, uint32 a)
{
	switch (op)
	{
	case TokenType::MINUS:
		if (kind == PrimitiveKind::FLOAT)
			return FloatToBits(-BitsToFloat(a));
		return 0u - a;
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

	if (op == TokenType::PLUS)
	{
		*out = a + b;
		return true;
	}
	if (op == TokenType::MINUS)
	{
		*out = a - b;
		return true;
	}
	if (op == TokenType::ASTERISK)
	{
		*out = a * b;
		return true;
	}

	if (b == 0)
	{
		EmitError(typer, "division by zero");
		return false;
	}
	if (kind == PrimitiveKind::SIGNED)
	{
		int32 x = (int32)a;
		int32 y = (int32)b;
		if (x == INT32_MIN && y == -1)
		{
			EmitError(typer, "integer overflow");
			return false;
		}
		if (op == TokenType::SLASH)
			*out = (uint32)(x / y);
		else
			*out = (uint32)(x % y);
		return true;
	}
	if (op == TokenType::SLASH)
		*out = a / b;
	else
		*out = a % b;
	return true;
}

// The typed node's value, or nullptr if it isn't a constant. Only errors for invalid operations, like division by zero.
static Constant* EvaluateConstant(Typer& typer, Node* node)
{
	CHECK_RECURSION(typer);
	if (!node->type)
		return nullptr;

	switch (node->node_type)
	{
	case NodeType::NUMBER:
	{
		uint32 bits = 0;
		if (!ParseNumber(typer, node, (PrimitiveType*)node->type, &bits))
			return nullptr;
		Constant* constant = NewConstant(typer, node->type);
		WriteComponent(constant, 0, bits);
		return constant;
	}
	case NodeType::REFERENCE:
	{
		if (node->target->kind == ElementKind::CONSTANT)
			return (Constant*)node->target;
		if (node->target->kind == ElementKind::NODE && ((Node*)node->target)->node_type == NodeType::CONST)
			return ((Node*)node->target)->constant;
		return nullptr;
	}
	case NodeType::UNARY:
	{
		Constant* operand = EvaluateConstant(typer, FindChild(node, Usage::OPERAND));
		if (!operand)
			return nullptr;
		Constant* constant = NewConstant(typer, node->type);
		PrimitiveKind kind = ComponentType(node->type)->primitive_kind;
		for (uint32 i = 0; i < ComponentCount(node->type); ++i)
			WriteComponent(constant, i, FoldUnary(kind, node->op, ReadComponent(operand, i)));
		return constant;
	}
	case NodeType::BINARY:
	{
		Constant* left = EvaluateConstant(typer, FindChild(node, Usage::LEFT));
		Constant* right = EvaluateConstant(typer, FindChild(node, Usage::RIGHT));
		if (!left || !right)
			return nullptr;
		Constant* constant = NewConstant(typer, node->type);
		PrimitiveKind kind = ComponentType(node->type)->primitive_kind;
		for (uint32 i = 0; i < ComponentCount(node->type); ++i)
		{
			uint32 bits = 0;
			if (!FoldBinary(typer, kind, node->op, ReadComponent(left, i), ReadComponent(right, i), &bits))
				return nullptr;
			WriteComponent(constant, i, bits);
		}
		return constant;
	}
	case NodeType::CALL:
	{
		// Vector constructors: the arguments' components in order, or one scalar for all of them.
		Constant* constant = NewConstant(typer, node->type);
		uint32 offset = 0;
		for (Node* argument = node->child; argument; argument = argument->next)
		{
			if (argument->usage != Usage::ARGUMENT)
				continue;
			Constant* value = EvaluateConstant(typer, argument);
			if (!value)
				return nullptr;
			memcpy(constant->bytes.data + offset, value->bytes.data, value->bytes.count);
			offset += value->bytes.count;
		}
		uint32 component_count = ComponentCount(node->type);
		if (offset == 4 && component_count > 1)
		{
			for (uint32 i = 1; i < component_count; ++i)
				WriteComponent(constant, i, ReadComponent(constant, 0));
		}
		return constant;
	}
	case NodeType::INDEX:
	{
		Node* object = FindChild(node, Usage::OBJECT);
		Constant* array = EvaluateConstant(typer, object);
		Constant* index = EvaluateConstant(typer, FindChild(node, Usage::INDEX));
		if (!array || !index)
			return nullptr;
		Constant* constant = NewConstant(typer, node->type);
		uint32 offset = (uint32)ConstantToInteger(index) * ((ArrayType*)object->type)->stride; // checked by TypeIndex
		memcpy(constant->bytes.data, array->bytes.data + offset, constant->bytes.count);
		return constant;
	}
	case NodeType::INIT_LIST:
	{
		Constant* constant = NewConstant(typer, node->type);
		uint32 index = 0;
		for (Node* element = node->child; element; element = element->next)
		{
			if (element->usage != Usage::ELEMENT)
				continue;
			Constant* value = EvaluateConstant(typer, element);
			if (!value)
				return nullptr;
			uint32 offset = 0;
			if (node->type->type_kind == TypeKind::ARRAY)
				offset = index * ((ArrayType*)node->type)->stride;
			else
				offset = ((StructType*)node->type)->fields[index].offset;
			memcpy(constant->bytes.data + offset, value->bytes.data, value->bytes.count);
			index++;
		}
		return constant;
	}
	default: return nullptr;
	}
}

// EvaluateConstant, erroring if the node isn't a constant.
static Constant* RequireConstant(Typer& typer, Node* node, const char* what)
{
	size_t error_count = typer.errors.size();
	Constant* constant = EvaluateConstant(typer, node);
	if (!constant && typer.errors.size() == error_count)
		EmitError(typer, "%s must be a constant", what);
	return constant;
}

static bool TypeNode(Typer& typer, Node* node, Type* expected);
static Type* EvaluateType(Typer& typer, Node* node);

// The only argument of an attribute, or nullptr with an error.
static Node* SingleArgument(Typer& typer, Node* attribute, const char* name)
{
	Node* argument = attribute->node_type == NodeType::CALL ? FindChild(attribute, Usage::ARGUMENT) : nullptr;
	if (!argument || (argument->next && argument->next->usage == Usage::ARGUMENT))
	{
		EmitError(typer, "'%s' takes one argument", name);
		return nullptr;
	}
	return argument;
}

static bool TypeAttribute(Typer& typer, Node* attribute, Node* target)
{
	Node* callee = attribute->node_type == NodeType::CALL ? FindChild(attribute, Usage::CALLEE) : attribute;
	if (callee->node_type != NodeType::REFERENCE)
		return false; // unresolved, already reported
	if (callee->target->kind != ElementKind::INTRINSIC)
	{
		EmitError(typer, "'%s' isn't an attribute", AtomName(typer, callee->name));
		return false;
	}

	Intrinsic* intrinsic = (Intrinsic*)callee->target;
	const char* name = AtomName(typer, intrinsic->name);
	switch (intrinsic->intrinsic_kind)
	{
	case IntrinsicKind::BUILTIN:
	case IntrinsicKind::LOCATION:
	{
		if (target->node_type != NodeType::PARAMETER && target->node_type != NodeType::FIELD &&
			target->usage != Usage::RETURN_TYPE)
		{
			EmitError(typer, "'%s' can only be used on parameters, fields and return types", name);
			return false;
		}
		Node* argument = SingleArgument(typer, attribute, name);
		if (!argument)
			return false;
		Type* argument_type = typer.context->uint_type;
		if (intrinsic->intrinsic_kind == IntrinsicKind::BUILTIN)
			argument_type = typer.context->builtin_type;
		if (!TypeNode(typer, argument, argument_type))
			return false;
		if (!ImplicitCast(typer, argument, argument_type))
			return false;
		if (intrinsic->intrinsic_kind == IntrinsicKind::LOCATION && !RequireConstant(typer, argument, "a location"))
			return false;
		return true;
	}
	case IntrinsicKind::VERTEX:
	case IntrinsicKind::FRAGMENT:
	{
		if (target->node_type != NodeType::FUNCTION)
		{
			EmitError(typer, "'%s' can only be used on functions", name);
			return false;
		}
		if (attribute->node_type == NodeType::CALL)
		{
			EmitError(typer, "'%s' takes no arguments", name);
			return false;
		}
		return true;
	}
	case IntrinsicKind::NONE: break;
	}
	return false;
}

static bool TypeAttributes(Typer& typer, Node* node)
{
	bool typed = true;
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->usage == Usage::ATTRIBUTE)
			typed = TypeAttribute(typer, child, node) && typed;
	}
	return typed;
}

// Types the struct's fields and lays them out, the first time it's needed.
static bool CompleteStruct(Typer& typer, StructType* type)
{
	CHECK_RECURSION(typer);
	if (type->state == StructState::COMPLETE)
		return true;
	if (type->state == StructState::FAILED)
		return false;
	if (type->state == StructState::COMPLETING)
	{
		EmitError(typer, "'%s' contains itself", AtomName(typer, type->name));
		return false;
	}
	type->state = StructState::COMPLETING;

	uint32 count = 0;
	for (Node* field = type->declaration->child; field; field = field->next)
	{
		if (field->usage == Usage::MEMBER)
			count++;
	}
	StructField* fields = (StructField*)typer.arena->Allocate(count * sizeof(StructField), alignof(StructField));

	bool typed = true;
	uint64 offset = 0;
	uint32 alignment = 1;
	uint32 index = 0;
	for (Node* field = type->declaration->child; field; field = field->next)
	{
		if (field->usage != Usage::MEMBER)
			continue;
		typed = TypeAttributes(typer, field) && typed;
		if (FindChild(field, Usage::VALUE))
		{
			EmitError(typer, "default values aren't supported yet");
			typed = false;
		}
		Type* field_type = EvaluateType(typer, FindChild(field, Usage::DECLARED_TYPE));
		field->type = field_type;
		if (!field_type)
		{
			typed = false;
			continue;
		}
		offset = (offset + field_type->alignment - 1) / field_type->alignment * field_type->alignment;
		fields[index] = { .name = field->name, .type = field_type, .offset = (uint32)offset, .declaration = field };
		index++;
		offset += field_type->size;
		if (field_type->alignment > alignment)
			alignment = field_type->alignment;
	}

	offset = (offset + alignment - 1) / alignment * alignment;
	if (offset > UINT32_MAX)
	{
		EmitError(typer, "'%s' is too large", AtomName(typer, type->name));
		typed = false;
	}
	type->fields = Slice<StructField>(fields, index);
	type->size = (uint32)offset;
	type->alignment = alignment;
	type->state = typed ? StructState::COMPLETE : StructState::FAILED;
	return typed;
}

// The type a type expression names, also stored in the node.
static Type* EvaluateType(Typer& typer, Node* node)
{
	CHECK_RECURSION(typer);
	TypeAttributes(typer, node);

	Type* type = nullptr;
	if (node->node_type == NodeType::REFERENCE && node->target->kind == ElementKind::TYPE)
	{
		type = (Type*)node->target;
		if (type->type_kind == TypeKind::STRUCT)
		{
			if (!CompleteStruct(typer, (StructType*)type))
				return nullptr;
		}
	}
	else if (node->node_type == NodeType::ARRAY_TYPE)
	{
		Node* size = FindChild(node, Usage::SIZE);
		Type* element = EvaluateType(typer, FindChild(node, Usage::ELEMENT));
		if (!TypeNode(typer, size, typer.context->uint_type))
			return nullptr;
		PrimitiveType* size_type = ComponentType(size->type);
		if (!size_type || ComponentCount(size->type) != 1 || !IsInteger(size_type))
		{
			EmitError(typer, "array size must be an int or uint, got %s", TypeName(typer, size->type));
			return nullptr;
		}
		Constant* length = RequireConstant(typer, size, "array size");
		if (!length || !element)
			return nullptr;
		int64 value = ConstantToInteger(length);
		if (value < 1)
		{
			EmitError(typer, "array size must be at least 1, got %lld", (long long)value);
			return nullptr;
		}
		type = GetArrayType(*typer.context, element, (uint32)value);
		if (!type)
		{
			EmitError(typer, "[%lld]%s is too large", (long long)value, TypeName(typer, element));
			return nullptr;
		}
	}
	else if (node->node_type != NodeType::IDENTIFIER) // unresolved, already reported
	{
		EmitError(typer, "expected a type");
	}
	node->type = type;
	return type;
}

static bool TypeConst(Typer& typer, Node* node)
{
	Node* declared = FindChild(node, Usage::DECLARED_TYPE);
	Node* value = FindChild(node, Usage::VALUE);
	Type* type = nullptr;
	if (declared)
	{
		type = EvaluateType(typer, declared);
		if (!type)
			return false;
	}
	if (!TypeNode(typer, value, type))
		return false;
	if (type && !ImplicitCast(typer, value, type))
		return false;
	node->type = type ? type : value->type;
	node->constant = RequireConstant(typer, value, "a const's value");
	return node->constant != nullptr;
}

static bool TypeFunction(Typer& typer, Node* node)
{
	bool typed = true;
	for (Node* parameter = node->child; parameter; parameter = parameter->next)
	{
		if (parameter->usage != Usage::PARAMETER)
			continue;
		typed = TypeAttributes(typer, parameter) && typed;
		if (FindChild(parameter, Usage::VALUE))
		{
			EmitError(typer, "default values aren't supported yet");
			typed = false;
		}
		parameter->type = EvaluateType(typer, FindChild(parameter, Usage::DECLARED_TYPE));
		typed = parameter->type != nullptr && typed;
	}

	Type* return_type = typer.context->void_type;
	if (Node* return_node = FindChild(node, Usage::RETURN_TYPE))
		return_type = EvaluateType(typer, return_node);
	if (!return_type)
		typed = false;

	Type* outer = typer.return_type;
	typer.return_type = return_type;
	DEFER(typer.return_type = outer);

	if (!TypeNode(typer, FindChild(node, Usage::BODY), nullptr))
		typed = false;

	return typed;
}

static bool TypeReturn(Typer& typer, Node* node)
{
	Node* value = FindChild(node, Usage::VALUE);
	Type* return_type = typer.return_type;
	if (!return_type)
	{
		// The function's return type failed, already reported. The value is still typed for its own errors.
		if (value)
			TypeNode(typer, value, nullptr);
		return false;
	}

	if (return_type == typer.context->void_type)
	{
		if (!value)
			return true;
		EmitError(typer, "a function returning void can't return a value");
		return false;
	}
	if (!value)
	{
		EmitError(typer, "'return' needs a value of type %s", TypeName(typer, return_type));
		return false;
	}
	if (!TypeNode(typer, value, return_type))
		return false;
	if (!ImplicitCast(typer, value, return_type))
		return false;
	return true;
}

static bool TypeNumber(Typer& typer, Node* node, Type* expected)
{
	PrimitiveType* type = typer.context->int_type;
	if (expected && expected->type_kind == TypeKind::PRIMITIVE && IsNumeric((PrimitiveType*)expected))
		type = (PrimitiveType*)expected;
	else if (LooksLikeFloat(node->text))
		type = typer.context->float_type;
	uint32 bits = 0;
	if (!ParseNumber(typer, node, type, &bits))
		return false;
	node->type = type;
	return true;
}

static bool TypeReference(Typer& typer, Node* node)
{
	const char* name = AtomName(typer, node->name);
	switch (node->target->kind)
	{
	case ElementKind::NODE:
	{
		Node* target = (Node*)node->target;
		switch (target->node_type)
		{
		case NodeType::CONST:
		case NodeType::PARAMETER:
		case NodeType::VARIABLE:
		case NodeType::ENUM_VALUE:
			node->type = target->type;
			return node->type != nullptr; // nullptr if the declaration failed, already reported
		case NodeType::FUNCTION:
			EmitError(typer, "'%s' is a function, which can only be called", name);
			return false;
		default:
			EmitError(typer, "'%s' can't be used as a value", name);
			return false;
		}
	}
	case ElementKind::TYPE:
		EmitError(typer, "'%s' is a type, not a value", name);
		return false;
	case ElementKind::INTRINSIC:
		EmitError(typer, "'%s' can only be used as an attribute", name);
		return false;
	case ElementKind::CONSTANT:
		node->type = ((Constant*)node->target)->type;
		return true;
	case ElementKind::NONE: break;
	}
	return false;
}

static bool TypeInitList(Typer& typer, Node* node, Type* expected)
{
	if (!expected)
	{
		EmitError(typer, "can't tell the type of an initializer list here");
		return false;
	}

	uint32 count = 0;
	for (Node* element = node->child; element; element = element->next)
	{
		if (element->usage == Usage::ELEMENT)
			count++;
	}

	uint32 expected_count = 0;
	if (expected->type_kind == TypeKind::ARRAY)
		expected_count = ((ArrayType*)expected)->length;
	else if (expected->type_kind == TypeKind::STRUCT)
		expected_count = ((StructType*)expected)->fields.count;
	else
	{
		EmitError(typer, "can't initialize %s with an initializer list", TypeName(typer, expected));
		return false;
	}
	if (count != expected_count)
	{
		EmitError(typer, "%s needs %u elements, got %u", TypeName(typer, expected), expected_count, count);
		return false;
	}

	bool typed = true;
	uint32 index = 0;
	for (Node* element = node->child; element; element = element->next)
	{
		if (element->usage != Usage::ELEMENT)
			continue;
		Type* element_type = nullptr;
		if (expected->type_kind == TypeKind::ARRAY)
			element_type = ((ArrayType*)expected)->element;
		else
			element_type = ((StructType*)expected)->fields[index].type;
		index++;

		if (!TypeNode(typer, element, element_type))
			typed = false;
		else if (!ImplicitCast(typer, element, element_type))
			typed = false;
	}
	node->type = expected;
	return typed;
}

static bool TypeUnary(Typer& typer, Node* node, Type* expected)
{
	if (node->op != TokenType::MINUS && node->op != TokenType::PLUS && node->op != TokenType::TILDE)
	{
		EmitError(typer, "'%s' isn't supported yet", TokenToString(node->op).CString());
		return false;
	}

	Node* operand = FindChild(node, Usage::OPERAND);
	if (!TypeNode(typer, operand, expected))
		return false;
	PrimitiveType* component = ComponentType(operand->type);
	bool valid = false;
	if (component && node->op == TokenType::TILDE)
		valid = IsInteger(component);
	else if (component && node->op == TokenType::MINUS)
		valid = component->primitive_kind == PrimitiveKind::SIGNED || component->primitive_kind == PrimitiveKind::FLOAT;
	else if (component)
		valid = IsNumeric(component);
	if (!valid)
	{
		EmitError(typer, "can't apply '%s' to %s", TokenToString(node->op).CString(), TypeName(typer, operand->type));
		return false;
	}
	node->type = operand->type;
	return true;
}

static bool TypeBinary(Typer& typer, Node* node, Type* expected)
{
	switch (node->op)
	{
	case TokenType::PLUS:
	case TokenType::MINUS:
	case TokenType::ASTERISK:
	case TokenType::SLASH:
	case TokenType::PERCENT: break;
	default:
		EmitError(typer, "'%s' isn't supported yet", TokenToString(node->op).CString());
		return false;
	}

	// Literals take the other side's type, so the other side goes first.
	Node* left = FindChild(node, Usage::LEFT);
	Node* right = FindChild(node, Usage::RIGHT);
	bool left_literal = left->node_type == NodeType::NUMBER;
	bool right_literal = right->node_type == NodeType::NUMBER;
	if (left_literal && right_literal && !expected && (LooksLikeFloat(left->text) || LooksLikeFloat(right->text)))
		expected = typer.context->float_type;
	Node* first = left;
	Node* second = right;
	if (left_literal && !right_literal)
	{
		first = right;
		second = left;
	}
	if (!TypeNode(typer, first, expected))
		return false;
	if (!TypeNode(typer, second, first->type))
		return false;
	if (!ImplicitCoCast(typer, left, right))
		return false;

	PrimitiveType* component = ComponentType(left->type);
	if (!component || !IsNumeric(component))
	{
		EmitError(typer, "can't apply '%s' to %s", TokenToString(node->op).CString(), TypeName(typer, left->type));
		return false;
	}
	node->type = left->type;
	return true;
}

static bool TypeConstructor(Typer& typer, Node* node, Type* type)
{
	if (type->type_kind != TypeKind::VECTOR)
	{
		EmitError(typer, "constructing %s isn't supported yet", TypeName(typer, type));
		return false;
	}
	VectorType* vector = (VectorType*)type;

	bool typed = true;
	uint32 components = 0;
	for (Node* argument = node->child; argument; argument = argument->next)
	{
		if (argument->usage != Usage::ARGUMENT)
			continue;
		if (!TypeNode(typer, argument, vector->element))
		{
			typed = false;
			continue;
		}
		if (TryImplicitCast(typer, argument, vector->element))
			components++;
		else if (argument->type->type_kind == TypeKind::VECTOR && ((VectorType*)argument->type)->element == vector->element)
			components += ((VectorType*)argument->type)->count;
		else
		{
			EmitError(typer, "can't construct %s from %s", TypeName(typer, type), TypeName(typer, argument->type));
			typed = false;
		}
	}
	if (typed && components != vector->count && components != 1)
	{
		EmitError(typer, "%s needs %u components, got %u", TypeName(typer, type), vector->count, components);
		typed = false;
	}
	if (typed)
		node->type = type;
	return typed;
}

static bool TypeCall(Typer& typer, Node* node)
{
	Node* callee = FindChild(node, Usage::CALLEE);
	if (callee->node_type != NodeType::REFERENCE)
		return false; // unresolved, already reported

	if (callee->target->kind == ElementKind::TYPE)
	{
		callee->type = (Type*)callee->target;
		return TypeConstructor(typer, node, callee->type);
	}
	if (callee->target->kind == ElementKind::NODE && ((Node*)callee->target)->node_type == NodeType::FUNCTION)
	{
		EmitError(typer, "calling functions isn't supported yet");
		return false;
	}
	if (!TypeNode(typer, callee, nullptr))
		return false;
	EmitError(typer, "%s can't be called", TypeName(typer, callee->type));
	return false;
}

static bool TypeIndex(Typer& typer, Node* node)
{
	Node* object = FindChild(node, Usage::OBJECT);
	Node* index = FindChild(node, Usage::INDEX);
	bool object_typed = TypeNode(typer, object, nullptr);
	bool index_typed = TypeNode(typer, index, typer.context->uint_type);
	if (!object_typed || !index_typed)
		return false;

	if (object->type->type_kind != TypeKind::ARRAY)
	{
		EmitError(typer, "can't index %s", TypeName(typer, object->type));
		return false;
	}
	ArrayType* array = (ArrayType*)object->type;
	PrimitiveType* index_type = ComponentType(index->type);
	if (!index_type || ComponentCount(index->type) != 1 || !IsInteger(index_type))
	{
		EmitError(typer, "index must be an int or uint, got %s", TypeName(typer, index->type));
		return false;
	}
	if (Constant* constant = EvaluateConstant(typer, index))
	{
		int64 value = ConstantToInteger(constant);
		if (value < 0 || value >= array->length)
		{
			EmitError(typer, "index %lld is out of bounds for %s", (long long)value, TypeName(typer, array));
			return false;
		}
	}
	node->type = array->element;
	return true;
}

static bool TypeMember(Typer& typer, Node* node)
{
	Node* object = FindChild(node, Usage::OBJECT);
	if (!TypeNode(typer, object, nullptr))
		return false;
	if (object->type->type_kind == TypeKind::STRUCT)
	{
		StructType* type = (StructType*)object->type;
		for (uint32 i = 0; i < type->fields.count; ++i)
		{
			if (type->fields[i].name == node->name)
			{
				node->type = type->fields[i].type;
				return true;
			}
		}
	}
	EmitError(typer, "%s has no member '%s'", TypeName(typer, object->type), AtomName(typer, node->name));
	return false;
}

// expected is the type the parent wants, if it knows. It's only a hint: literals take it, and initializer lists need it.
// The parent checks the result against it.
static bool TypeNode(Typer& typer, Node* node, Type* expected)
{
	CHECK_RECURSION(typer);
	bool typed = TypeAttributes(typer, node);

	switch (node->node_type)
	{
	case NodeType::MODULE:
	case NodeType::BLOCK:
	{
		for (Node* child = node->child; child; child = child->next)
		{
			if (child->usage != Usage::ATTRIBUTE)
				typed = TypeNode(typer, child, nullptr) && typed;
		}
		break;
	}
	case NodeType::CONST:
		if (!TypeConst(typer, node))
			typed = false;
		break;
	case NodeType::STRUCT:
		if (!CompleteStruct(typer, (StructType*)node->type))
			typed = false;
		break;
	case NodeType::FUNCTION:
		if (!TypeFunction(typer, node))
			typed = false;
		break;
	case NodeType::VARIABLE:
		node->type = EvaluateType(typer, FindChild(node, Usage::DECLARED_TYPE));
		if (!node->type)
			typed = false;
		break;
	case NodeType::RETURN:
		if (!TypeReturn(typer, node))
			typed = false;
		break;
	case NodeType::NUMBER:
		if (!TypeNumber(typer, node, expected))
			typed = false;
		break;
	case NodeType::REFERENCE:
		if (!TypeReference(typer, node))
			typed = false;
		break;
	case NodeType::INIT_LIST:
		if (!TypeInitList(typer, node, expected))
			typed = false;
		break;
	case NodeType::UNARY:
		if (!TypeUnary(typer, node, expected))
			typed = false;
		break;
	case NodeType::BINARY:
		if (!TypeBinary(typer, node, expected))
			typed = false;
		break;
	case NodeType::CALL:
		if (!TypeCall(typer, node))
			typed = false;
		break;
	case NodeType::INDEX:
		if (!TypeIndex(typer, node))
			typed = false;
		break;
	case NodeType::MEMBER:
		if (!TypeMember(typer, node))
			typed = false;
		break;
	case NodeType::IDENTIFIER:
		typed = false; // unresolved, already reported
		break;
	case NodeType::ARRAY_TYPE:
		EmitError(typer, "expected a value, got a type");
		typed = false;
		break;
	case NodeType::BOOL:
		EmitError(typer, "bool isn't supported yet");
		typed = false;
		break;
	case NodeType::IF:
		EmitError(typer, "'if' isn't supported yet");
		typed = false;
		break;
	case NodeType::POSTFIX:
		EmitError(typer, "'%s' isn't supported yet", TokenToString(node->op).CString());
		typed = false;
		break;
	default:
		EmitError(typer, "unexpected %s", NodeTypeToString(node->node_type).CString());
		typed = false;
		break;
	}
	return typed;
}

bool TypeCheck(Typer& typer, Node* module)
{
	TypeNode(typer, module, nullptr);
	return typer.errors.empty();
}

}
