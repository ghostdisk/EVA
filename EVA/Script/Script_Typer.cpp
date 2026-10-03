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

PrimitiveType* ComponentType(Type* type)
{
	if (type->type_kind == TypeKind::PRIMITIVE)
		return (PrimitiveType*)type;
	if (type->type_kind == TypeKind::VECTOR)
		return ((VectorType*)type)->element;
	return nullptr;
}

uint32 ComponentCount(Type* type)
{
	return type->type_kind == TypeKind::VECTOR ? ((VectorType*)type)->count : 1;
}

bool IsNumeric(PrimitiveType* type)
{
	return type->primitive_kind == PrimitiveKind::SIGNED || type->primitive_kind == PrimitiveKind::UNSIGNED ||
		   type->primitive_kind == PrimitiveKind::FLOAT;
}

bool IsInteger(PrimitiveType* type)
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

static const char* NumberName(Typer& typer, NumberLiteral* number)
{
	return NumberToString(number, typer.arena).CString();
}

// Whether the integer has at most bits significant bits, so a float with a mantissa that wide holds it exactly.
static bool FitsMantissa(uint64 value, uint32 bits)
{
	while (value && !(value & 1))
		value >>= 1;
	return value < ((uint64)1 << bits);
}

// Errors if the literal's value doesn't fit type, which is int, uint or float. Integers only go to floats exactly.
static bool CheckNumberFits(Typer& typer, NumberLiteral* number, PrimitiveType* type)
{
	if (type->primitive_kind == PrimitiveKind::FLOAT)
	{
		if (number->kind == NumberKind::FLOAT && isinf(number->f32))
		{
			EmitError(typer, "'%s' is out of range for float", NumberName(typer, number));
			return false;
		}
		if (number->kind == NumberKind::INTEGER && !FitsMantissa(number->integer, 24))
		{
			EmitError(typer, "'%s' can't be represented exactly as a float", NumberName(typer, number));
			return false;
		}
		return true;
	}

	if (number->kind == NumberKind::FLOAT)
	{
		EmitError(typer, "'%s' is not an integer", NumberName(typer, number));
		return false;
	}
	uint64 limit = UINT32_MAX;
	if (type->primitive_kind == PrimitiveKind::SIGNED)
		limit = INT32_MAX;
	if (number->integer > limit)
	{
		EmitError(typer, "'%s' is out of range for %s", NumberName(typer, number), TypeName(typer, type));
		return false;
	}
	return true;
}

static bool TypeNode(Typer& typer, Node* node, Type* expected);

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

// One of type's values, the only names visible in the argument.
static bool TypeEnumArgument(Typer& typer, Node* argument, EnumType* type)
{
	if (!TypeNode(typer, argument, type))
		return false;
	return ImplicitCast(typer, argument, type) != nullptr;
}

static bool TypeAttribute(Typer& typer, Node* attribute, Node* target)
{
	Node* callee = attribute->node_type == NodeType::CALL ? FindChild(attribute, Usage::CALLEE) : attribute;
	if (callee->node_type == NodeType::IDENTIFIER)
		return false; // unresolved, already reported
	if (callee->node_type != NodeType::REFERENCE)
	{
		EmitError(typer, "expected an attribute name");
		return false;
	}
	if (callee->target->kind != ElementKind::INTRINSIC)
	{
		EmitError(typer, "'%s' isn't an attribute", AtomName(typer, callee->name));
		return false;
	}

	Intrinsic* intrinsic = (Intrinsic*)callee->target;
	const char* name = AtomName(typer, intrinsic->name);
	switch (intrinsic->intrinsic_kind)
	{
	case IntrinsicKind::SEMANTIC:
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
		if (intrinsic->intrinsic_kind == IntrinsicKind::SEMANTIC)
			return TypeEnumArgument(typer, argument, typer.context->semantic_type);
		Constant* location = EvaluateConstant(typer, argument, typer.context->uint_type, "a location");
		if (!location)
			return false;
		if (location->type != typer.context->uint_type)
		{
			EmitError(typer, "expected uint, got %s", TypeName(typer, location->type));
			return false;
		}
		return true;
	}
	case IntrinsicKind::ENTRY:
	{
		if (target->node_type != NodeType::FUNCTION)
		{
			EmitError(typer, "'%s' can only be used on functions", name);
			return false;
		}
		Node* argument = SingleArgument(typer, attribute, name);
		return argument && TypeEnumArgument(typer, argument, typer.context->stage_type);
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
		assert(index < count);
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

// A constant argument of a generic as its parameter's type: an integer converts to another integer type if the value
// fits, so Array(float, 3) and Array(float, 3u) are one type. nullptr with an error otherwise.
static Constant* ConvertGenericConstant(Typer& typer, const GenericParam& param, Constant* constant)
{
	if (constant->type == param.type)
		return constant;
	PrimitiveType* component = ComponentType(constant->type);
	if (!component || ComponentCount(constant->type) != 1 || !IsInteger(component))
	{
		EmitError(typer, "%s must be an int or uint, got %s", param.what, TypeName(typer, constant->type));
		return nullptr;
	}
	assert(IsInteger(param.type));
	int64 value = ConstantToInteger(constant);
	bool fits = param.type->primitive_kind == PrimitiveKind::SIGNED ? value >= INT32_MIN && value <= INT32_MAX
																   : value >= 0 && value <= UINT32_MAX;
	if (!fits)
	{
		EmitError(typer, "%s %lld is out of range for %s", param.what, (long long)value, TypeName(typer, param.type));
		return nullptr;
	}
	Constant* converted = typer.arena->New<Constant>();
	converted->type = param.type;
	uint32 bits = (uint32)value;
	uint8* bytes = (uint8*)typer.arena->Allocate(4, 4);
	memcpy(bytes, &bits, 4);
	converted->bytes = Slice<uint8>(bytes, 4);
	return converted;
}

// The most parameters a built-in generic has.
static const uint32 MAX_GENERIC_PARAMS = 4;

// Types arguments by generic's parameters and instantiates it. node, the CALL or ARRAY_TYPE, remembers the outcome, so
// another visit neither types the arguments again nor repeats their errors.
static bool InstantiateNode(Typer& typer, Node* node, Generic* generic, Slice<Node*> arguments, Element** out)
{
	if (node->typing_state == TypingState::TYPED)
	{
		*out = node->type;
		return true;
	}
	if (node->typing_state == TypingState::FAILED)
		return false;
	node->typing_state = TypingState::FAILED; // until it succeeds

	uint32 count = generic->params.count;
	assert(count <= MAX_GENERIC_PARAMS);
	if (arguments.count != count)
	{
		EmitError(typer, "'%s' takes %u argument%s, got %u", AtomName(typer, generic->name), count, count == 1 ? "" : "s",
			arguments.count);
		return false;
	}

	GenericArg args[MAX_GENERIC_PARAMS];
	bool typed = true;
	for (uint32 i = 0; i < count; ++i)
	{
		const GenericParam& param = generic->params[i];
		if (param.kind == GenericParamKind::TYPE)
		{
			args[i].type = EvaluateType(typer, arguments[i]);
			typed = args[i].type != nullptr && typed;
			continue;
		}
		Constant* constant = EvaluateConstant(typer, arguments[i], param.type, param.what);
		args[i].constant = constant ? ConvertGenericConstant(typer, param, constant) : nullptr;
		typed = args[i].constant != nullptr && typed;
	}
	if (!typed)
		return false;

	Type* type = Instantiate(*typer.context, generic, Slice<GenericArg>(args, count), &typer);
	if (!type)
		return false;
	node->type = type;
	node->typing_state = TypingState::TYPED;
	*out = type;
	return true;
}

static bool TypeAlias(Typer& typer, Node* node);

bool ResolveName(Typer& typer, Node* node, Element** out)
{
	*out = nullptr;
	switch (node->node_type)
	{
	case NodeType::REFERENCE:
	{
		Element* target = node->target;
		if (target->kind == ElementKind::NODE && ((Node*)target)->node_type == NodeType::TYPE_ALIAS)
		{
			// An alias stands for its type, typed on demand.
			if (!TypeAlias(typer, (Node*)target))
				return false;
			target = ((Node*)target)->type;
		}
		*out = target;
		return true;
	}
	case NodeType::IDENTIFIER: return false; // unresolved, already reported
	case NodeType::CALL:
	{
		CHECK_RECURSION(typer); // through the callee, a chain of calls can be long
		Node* callee = FindChild(node, Usage::CALLEE);
		Element* target = nullptr;
		if (!ResolveName(typer, callee, &target))
			return false;
		if (!target || target->kind != ElementKind::GENERIC)
			return true; // a constructor or a function call: a value
		Node* arguments[MAX_GENERIC_PARAMS];
		uint32 count = 0;
		for (Node* child = node->child; child; child = child->next)
		{
			if (child->usage != Usage::ARGUMENT)
				continue;
			if (count == MAX_GENERIC_PARAMS)
			{
				count = MAX_GENERIC_PARAMS + 1; // too many, whatever the generic
				break;
			}
			arguments[count++] = child;
		}
		return InstantiateNode(typer, node, (Generic*)target, Slice<Node*>(arguments, count), out);
	}
	case NodeType::ARRAY_TYPE:
	{
		Node* arguments[] = { FindChild(node, Usage::ELEMENT), FindChild(node, Usage::SIZE) };
		return InstantiateNode(typer, node, typer.context->array_generic, Slice<Node*>(arguments, 2), out);
	}
	default: return true;
	}
}

Type* EvaluateType(Typer& typer, Node* node)
{
	CHECK_RECURSION(typer);
	TypeAttributes(typer, node);

	Element* element = nullptr;
	if (!ResolveName(typer, node, &element))
		return nullptr;
	if (!element || element->kind != ElementKind::TYPE)
	{
		if (element && element->kind == ElementKind::GENERIC)
			EmitError(typer, "'%s' needs arguments", AtomName(typer, ((Generic*)element)->name));
		else
			EmitError(typer, "expected a type");
		return nullptr;
	}
	Type* type = (Type*)element;
	if (type->type_kind == TypeKind::STRUCT && !CompleteStruct(typer, (StructType*)type))
		return nullptr;
	node->type = type;
	return type;
}

// The value isn't typed, it's evaluated.
static bool TypeConstValue(Typer& typer, Node* node)
{
	Node* declared = FindChild(node, Usage::DECLARED_TYPE);
	Node* value = FindChild(node, Usage::VALUE);
	assert(value); // required by the parser
	Type* type = nullptr;
	if (declared)
	{
		type = EvaluateType(typer, declared);
		if (!type)
			return false;
	}
	Constant* constant = EvaluateConstant(typer, value, type, "a const's value");
	if (!constant)
		return false;
	if (type && constant->type != type)
	{
		EmitError(typer, "expected %s, got %s", TypeName(typer, type), TypeName(typer, constant->type));
		return false;
	}
	node->type = constant->type; // the value is now a CONSTANT
	return true;
}

// Types a declaration the first time it's needed, with type_value, which can be before its turn. Reaching it again while
// it's being typed is a cycle.
static bool TypeOnDemand(Typer& typer, Node* node, bool (*type_value)(Typer&, Node*))
{
	switch (node->typing_state)
	{
	case TypingState::TYPED: return true;
	case TypingState::FAILED: return false; // already reported
	case TypingState::TYPING:
		EmitError(typer, "'%s' depends on itself", AtomName(typer, node->name));
		return false;
	case TypingState::UNTYPED: break;
	}
	node->typing_state = TypingState::TYPING;
	bool typed = type_value(typer, node);
	node->typing_state = typed ? TypingState::TYPED : TypingState::FAILED;
	return typed;
}

bool TypeConst(Typer& typer, Node* node)
{
	return TypeOnDemand(typer, node, TypeConstValue);
}

static bool TypeAliasValue(Typer& typer, Node* node)
{
	node->type = EvaluateType(typer, FindChild(node, Usage::VALUE));
	return node->type != nullptr;
}

// Types a type alias, whose type is the one its value names.
static bool TypeAlias(Typer& typer, Node* node)
{
	return TypeOnDemand(typer, node, TypeAliasValue);
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

// let name [: type] [= value]: the type is the declared one, or the value's. A global's value is a constant, since
// nothing runs code to initialize globals yet.
static bool TypeVariable(Typer& typer, Node* node)
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
	if (value && node->usage == Usage::DECLARATION)
	{
		Constant* constant = EvaluateConstant(typer, value, type, "a global's value");
		if (!constant)
			return false;
		if (type && constant->type != type)
		{
			EmitError(typer, "expected %s, got %s", TypeName(typer, type), TypeName(typer, constant->type));
			return false;
		}
		type = constant->type;
	}
	else if (value)
	{
		if (!TypeNode(typer, value, type))
			return false;
		if (type && !ImplicitCast(typer, value, type))
			return false;
		type = value->type;
	}
	node->type = type;
	return true;
}

// A global is typed the first time it's needed: functions anywhere in the module can use it.
static bool TypeGlobal(Typer& typer, Node* node)
{
	return TypeOnDemand(typer, node, TypeVariable);
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

PrimitiveType* NumberType(Typer& typer, NumberLiteral* number, Type* expected)
{
	PrimitiveType* type = typer.context->int_type;
	if (expected && expected->type_kind == TypeKind::PRIMITIVE && IsNumeric((PrimitiveType*)expected))
		type = (PrimitiveType*)expected;
	else if (number->kind == NumberKind::FLOAT)
		type = typer.context->float_type;
	return CheckNumberFits(typer, number, type) ? type : nullptr;
}

static bool TypeNumber(Typer& typer, Node* node, Type* expected)
{
	node->type = NumberType(typer, node->number, expected);
	return node->type != nullptr;
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
			if (!TypeConst(typer, target))
				return false;
			node->type = target->type;
			return true;
		case NodeType::VARIABLE:
			if (target->usage == Usage::DECLARATION && !TypeGlobal(typer, target))
				return false; // reported
			node->type = target->type;
			return node->type != nullptr;
		case NodeType::PARAMETER:
		case NodeType::ENUM_VALUE:
			node->type = target->type;
			return node->type != nullptr; // nullptr if the declaration failed, already reported
		case NodeType::FUNCTION:
			EmitError(typer, "'%s' is a function, which can only be called", name);
			return false;
		case NodeType::TYPE_ALIAS:
			EmitError(typer, "'%s' is a type, not a value", name);
			return false;
		default:
			EmitError(typer, "'%s' can't be used as a value", name);
			return false;
		}
	}
	case ElementKind::TYPE:
	case ElementKind::GENERIC:
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

		if (!TypeNode(typer, element, element_type) || !ImplicitCast(typer, element, element_type))
			typed = false;
	}
	node->type = expected;
	return typed;
}

bool CheckOperator(Typer& typer, NodeType node_type, TokenType op)
{
	bool supported = false;
	if (node_type == NodeType::UNARY)
		supported = op == TokenType::MINUS || op == TokenType::PLUS || op == TokenType::TILDE;
	else
		supported = op == TokenType::PLUS || op == TokenType::MINUS || op == TokenType::ASTERISK || op == TokenType::SLASH ||
					op == TokenType::PERCENT;
	if (!supported)
		EmitError(typer, "'%s' isn't supported yet", TokenToString(op).CString());
	return supported;
}

bool CheckOperands(Typer& typer, NodeType node_type, TokenType op, Type* type)
{
	PrimitiveType* component = ComponentType(type);
	bool valid = false;
	if (component && node_type == NodeType::UNARY && op == TokenType::TILDE)
		valid = IsInteger(component);
	else if (component && node_type == NodeType::UNARY && op == TokenType::MINUS)
		valid = component->primitive_kind == PrimitiveKind::SIGNED || component->primitive_kind == PrimitiveKind::FLOAT;
	else if (component)
		valid = IsNumeric(component);
	if (!valid)
		EmitError(typer, "can't apply '%s' to %s", TokenToString(op).CString(), TypeName(typer, type));
	return valid;
}

static bool TypeUnary(Typer& typer, Node* node, Type* expected)
{
	if (!CheckOperator(typer, NodeType::UNARY, node->op))
		return false;
	Node* operand = FindChild(node, Usage::OPERAND);
	if (!TypeNode(typer, operand, expected) || !CheckOperands(typer, NodeType::UNARY, node->op, operand->type))
		return false;
	node->type = operand->type;
	return true;
}

static bool TypeBinary(Typer& typer, Node* node, Type* expected)
{
	if (!CheckOperator(typer, NodeType::BINARY, node->op))
		return false;

	// Literals take the other side's type, so the other side goes first.
	Node* left = FindChild(node, Usage::LEFT);
	Node* right = FindChild(node, Usage::RIGHT);
	bool left_literal = left->node_type == NodeType::NUMBER;
	bool right_literal = right->node_type == NodeType::NUMBER;
	if (left_literal && right_literal && !expected)
	{
		if (left->number->kind == NumberKind::FLOAT || right->number->kind == NumberKind::FLOAT)
			expected = typer.context->float_type;
	}
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
	if (!ImplicitCoCast(typer, left, right) || !CheckOperands(typer, NodeType::BINARY, node->op, left->type))
		return false;
	node->type = left->type;
	return true;
}

bool AddConstructorArgument(Typer& typer, VectorType* vector, Type* type, uint32* components)
{
	if (type == vector->element)
		*components += 1;
	else if (type->type_kind == TypeKind::VECTOR && ((VectorType*)type)->element == vector->element)
		*components += ((VectorType*)type)->count;
	else
	{
		EmitError(typer, "can't construct %s from %s", TypeName(typer, vector), TypeName(typer, type));
		return false;
	}
	return true;
}

bool CheckConstructorComponents(Typer& typer, VectorType* vector, uint32 components)
{
	if (components == vector->count || components == 1)
		return true;
	EmitError(typer, "%s needs %u components, got %u", TypeName(typer, vector), vector->count, components);
	return false;
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
		if (!TypeNode(typer, argument, vector->element) || !AddConstructorArgument(typer, vector, argument->type, &components))
			typed = false;
	}
	if (typed && !CheckConstructorComponents(typer, vector, components))
		typed = false;
	if (typed)
		node->type = type;
	return typed;
}

static bool TypeCall(Typer& typer, Node* node)
{
	Node* callee = FindChild(node, Usage::CALLEE);
	Element* target = nullptr;
	if (!ResolveName(typer, callee, &target))
		return false;
	switch (target ? target->kind : ElementKind::NONE)
	{
	case ElementKind::TYPE: // float3(...), and Array(...)(...) however the type is spelled
		callee->type = (Type*)target;
		return TypeConstructor(typer, node, callee->type);
	case ElementKind::GENERIC: // the call instantiates the generic, which makes a type, not a value
		EmitError(typer, "expected a value, got a type");
		return false;
	case ElementKind::NODE:
		if (((Node*)target)->node_type == NodeType::FUNCTION)
		{
			EmitError(typer, "calling functions isn't supported yet");
			return false;
		}
		break;
	default: break;
	}
	// The callee is a value: typed for its own errors, then it can't be called whatever it is.
	if (TypeNode(typer, callee, nullptr))
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
	if (Constant* constant = TryEvaluateConstant(typer, index, index->type))
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
	case NodeType::TYPE_ALIAS:
		if (!TypeAlias(typer, node))
			typed = false;
		break;
	case NodeType::VARIABLE:
		if (!(node->usage == Usage::DECLARATION ? TypeGlobal(typer, node) : TypeVariable(typer, node)))
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
	case NodeType::RETURN:
		if (!TypeReturn(typer, node))
			typed = false;
		break;
	case NodeType::NUMBER:
		if (!TypeNumber(typer, node, expected))
			typed = false;
		break;
	case NodeType::CONSTANT: break; // folded, already typed
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
	bool typed = TypeNode(typer, module, nullptr);
	assert(typed || !typer.errors.empty()); // every failure is reported
	return typer.errors.empty();
}

}
