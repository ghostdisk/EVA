#include <EVA/Script/Fuzz/FuzzCommon.hpp>
#include <EVA/Core/Panic.hpp>
#include <EVA/Script/Test/OutputValidation.hpp>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>
#include <vector>
#ifdef EVA_WIN32
#include <crtdbg.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef CONST // clashes with NodeType::CONST
#undef VOID  // and PrimitiveKind::VOID
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define EVA_ASAN 1
#endif
#endif
#ifdef EVA_ASAN
#include <sanitizer/common_interface_defs.h>
#endif

namespace EVA::Script::Fuzz
{

static ZTStringView current_source;

// Prints the source with anything unprintable escaped, so it can be pasted into a test.
static void PrintSource()
{
	fprintf(stderr, "---- source (%zu bytes) ----\n", current_source.length);
	for (size_t i = 0; i < current_source.length; ++i)
	{
		uint8 ch = current_source[i];
		if ((ch >= ' ' && ch <= '~') || ch == '\n' || ch == '\t')
			fputc(ch, stderr);
		else
			fprintf(stderr, "\\x%02X", ch);
	}
	fprintf(stderr, "\n---- end of source ----\n");
}

void Fail(const char* format, ...)
{
	fprintf(stderr, "\nfuzz check failed: ");
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	PrintSource();
#ifdef EVA_ASAN
	__sanitizer_print_stack_trace();
#endif
	fflush(stderr);
	abort();
}

static void PanicHandler(ZTStringView message)
{
	Fail("panic: %s", message.CString());
}

void InitFuzzing()
{
	SetPanicHandler(PanicHandler);
#ifdef EVA_WIN32
	// abort() and crashes would otherwise open dialogs and hang the fuzzer.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}

// Errors have to be in the output arena, readable after the intermediate one is gone, and printable: they end up in
// logs and terminals, so source text in them mustn't carry control characters.
static void CheckErrors(Compilation& compilation, const std::vector<ScriptError*>& errors, bool succeeded, const char* stage)
{
	if (!succeeded && errors.empty())
		Fail("%s failed without an error", stage);
	if (succeeded && !errors.empty())
		Fail("%s succeeded with %zu errors, the first: %s", stage, errors.size(), errors[0]->message.CString());

	for (ScriptError* error : errors)
	{
		if (!error || !compilation.output_arena->Contains(error))
			Fail("%s error isn't in the output arena", stage);
		if (error->error_family != ErrorFamily::SCRIPT_ERROR)
			Fail("%s error has family %u", stage, (uint32)error->error_family);
		ZTStringView message = error->message;
		if (!message.length)
			Fail("%s error has an empty message", stage);
		if (!compilation.output_arena->Contains(message.data))
			Fail("%s error's message isn't in the output arena: %s", stage, message.CString());
		if (message.CString()[message.length] != '\0')
			Fail("%s error's message isn't zero terminated", stage);
		for (size_t i = 0; i < message.length; ++i)
		{
			if (message[i] < ' ' || message[i] > '~')
				Fail("%s error's message has byte 0x%02X at %zu: %s", stage, message[i], i, message.CString());
		}
	}
}

static Slice<ScriptError*> CopyErrors(Arena* arena, const std::vector<ScriptError*>& errors)
{
	ScriptError** data = (ScriptError**)arena->Allocate(errors.size() * sizeof(ScriptError*), alignof(ScriptError*));
	for (size_t i = 0; i < errors.size(); ++i)
		data[i] = errors[i];
	return Slice<ScriptError*>(data, (uint32)errors.size());
}

// Tree shape

static bool IsExpression(NodeType type)
{
	switch (type)
	{
	case NodeType::NUMBER:
	case NodeType::BOOL:
	case NodeType::IDENTIFIER:
	case NodeType::REFERENCE:
	case NodeType::CONSTANT:
	case NodeType::INIT_LIST:
	case NodeType::UNARY:
	case NodeType::POSTFIX:
	case NodeType::BINARY:
	case NodeType::CALL:
	case NodeType::MEMBER:
	case NodeType::INDEX:
	case NodeType::ARRAY_TYPE:
	case NodeType::IF:
	case NodeType::VARIABLE: // made from a ':' expression by the resolver
		return true;
	default:
		return false;
	}
}

static bool IsStatement(NodeType type)
{
	return IsExpression(type) || type == NodeType::BLOCK || type == NodeType::RETURN || type == NodeType::CONST ||
		   type == NodeType::STRUCT || type == NodeType::FUNCTION;
}

static bool IsValidOperator(NodeType node_type, TokenType op)
{
	switch (node_type)
	{
	case NodeType::UNARY:
		return op == TokenType::MINUS || op == TokenType::PLUS || op == TokenType::EXCLAMATION || op == TokenType::TILDE ||
			   op == TokenType::INCREMENT || op == TokenType::DECREMENT;
	case NodeType::POSTFIX: return op == TokenType::INCREMENT || op == TokenType::DECREMENT;
	case NodeType::BINARY:
		switch (op)
		{
		case TokenType::ASTERISK:
		case TokenType::SLASH:
		case TokenType::PERCENT:
		case TokenType::PLUS:
		case TokenType::MINUS:
		case TokenType::SHIFT_LEFT:
		case TokenType::SHIFT_RIGHT:
		case TokenType::LESS:
		case TokenType::GREATER:
		case TokenType::LESS_EQUAL:
		case TokenType::GREATER_EQUAL:
		case TokenType::EQUAL:
		case TokenType::NOT_EQUAL:
		case TokenType::AMPERSAND:
		case TokenType::CARET:
		case TokenType::PIPE:
		case TokenType::LOGICAL_AND:
		case TokenType::LOGICAL_OR:
		case TokenType::COLON:
		case TokenType::EQUALS:
		case TokenType::ADD_ASSIGN:
		case TokenType::SUBTRACT_ASSIGN:
		case TokenType::MULTIPLY_ASSIGN:
		case TokenType::DIVIDE_ASSIGN:
		case TokenType::MODULO_ASSIGN:
		case TokenType::BIT_AND_ASSIGN:
		case TokenType::BIT_OR_ASSIGN:
		case TokenType::BIT_XOR_ASSIGN:
		case TokenType::SHIFT_LEFT_ASSIGN:
		case TokenType::SHIFT_RIGHT_ASSIGN: return true;
		default: return false;
		}
	default: return false;
	}
}

static const uint8 MANY = 255;

struct ChildRule
{
	Usage usage;
	uint8 min;
	uint8 max;
	bool (*allowed)(NodeType type); // nullptr: any expression
};

static bool IsDeclaration(NodeType type)
{
	return type == NodeType::CONST || type == NodeType::STRUCT || type == NodeType::FUNCTION;
}
static bool IsField(NodeType type) { return type == NodeType::FIELD; }
static bool IsParameter(NodeType type) { return type == NodeType::PARAMETER; }
static bool IsBlock(NodeType type) { return type == NodeType::BLOCK; }
static bool IsBranch(NodeType type) { return IsExpression(type) || type == NodeType::BLOCK; }

// The children each node type can have, by usage. Attributes are allowed on everything but MODULE and statements that
// aren't expressions.
static Slice<const ChildRule> ChildRules(NodeType type)
{
	static const ChildRule module[] = { { Usage::DECLARATION, 0, MANY, IsDeclaration } };
	static const ChildRule constant[] = { { Usage::DECLARED_TYPE, 0, 1 }, { Usage::VALUE, 1, 1 } };
	static const ChildRule structure[] = { { Usage::MEMBER, 0, MANY, IsField } };
	static const ChildRule function[] = {
		{ Usage::PARAMETER, 0, MANY, IsParameter }, { Usage::RETURN_TYPE, 0, 1 }, { Usage::BODY, 1, 1, IsBlock } };
	static const ChildRule typed_declaration[] = { { Usage::DECLARED_TYPE, 1, 1 }, { Usage::VALUE, 0, 1 } };
	static const ChildRule variable[] = { { Usage::DECLARED_TYPE, 1, 1 } };
	static const ChildRule block[] = { { Usage::STATEMENT, 0, MANY, IsStatement } };
	static const ChildRule return_statement[] = { { Usage::VALUE, 0, 1 } };
	static const ChildRule init_list[] = { { Usage::ELEMENT, 0, MANY } };
	static const ChildRule unary[] = { { Usage::OPERAND, 1, 1 } };
	static const ChildRule binary[] = { { Usage::LEFT, 1, 1 }, { Usage::RIGHT, 1, 1 } };
	static const ChildRule call[] = { { Usage::CALLEE, 1, 1 }, { Usage::ARGUMENT, 0, MANY } };
	static const ChildRule member[] = { { Usage::OBJECT, 1, 1 } };
	static const ChildRule index[] = { { Usage::OBJECT, 1, 1 }, { Usage::INDEX, 1, 1 } };
	static const ChildRule array_type[] = { { Usage::SIZE, 1, 1 }, { Usage::ELEMENT, 1, 1 } };
	static const ChildRule if_expression[] = {
		{ Usage::CONDITION, 1, 1 }, { Usage::THEN, 1, 1, IsBranch }, { Usage::ELSE, 0, 1, IsBranch } };

	switch (type)
	{
	case NodeType::MODULE: return module;
	case NodeType::CONST: return constant;
	case NodeType::STRUCT: return structure;
	case NodeType::FUNCTION: return function;
	case NodeType::PARAMETER:
	case NodeType::FIELD: return typed_declaration;
	case NodeType::VARIABLE: return variable;
	case NodeType::BLOCK: return block;
	case NodeType::RETURN: return return_statement;
	case NodeType::INIT_LIST: return init_list;
	case NodeType::UNARY:
	case NodeType::POSTFIX: return unary;
	case NodeType::BINARY: return binary;
	case NodeType::CALL: return call;
	case NodeType::MEMBER: return member;
	case NodeType::INDEX: return index;
	case NodeType::ARRAY_TYPE: return array_type;
	case NodeType::IF: return if_expression;
	default: return {};
	}
}

static bool CanHaveAttributes(NodeType type)
{
	return type != NodeType::MODULE && type != NodeType::BLOCK && type != NodeType::RETURN;
}

static bool NeedsName(NodeType type)
{
	switch (type)
	{
	case NodeType::CONST:
	case NodeType::STRUCT:
	case NodeType::FUNCTION:
	case NodeType::PARAMETER:
	case NodeType::FIELD:
	case NodeType::VARIABLE:
	case NodeType::IDENTIFIER:
	case NodeType::REFERENCE:
	case NodeType::MEMBER: return true;
	default: return false;
	}
}

static void CheckNodeShape(Node* node)
{
	const char* name = NodeTypeToString(node->node_type).CString();
	if (node->kind != ElementKind::NODE)
		Fail("%s has element kind %u", name, (uint32)node->kind);
	if (node->node_type == NodeType::NONE || node->node_type == NodeType::ENUM_VALUE || node->node_type > NodeType::IF)
		Fail("node type %u in the tree", (uint32)node->node_type);
	if (NeedsName(node->node_type) && node->name == Atom::NONE)
		Fail("%s without a name", name);

	switch (node->node_type)
	{
	case NodeType::NUMBER:
		if (!node->number)
			Fail("NUMBER without a literal");
		if (node->number->kind == NumberKind::FLOAT && !isfinite(node->number->f64))
			Fail("NUMBER %f isn't finite", node->number->f64);
		break;
	case NodeType::UNARY:
	case NodeType::POSTFIX:
	case NodeType::BINARY:
		if (!IsValidOperator(node->node_type, node->op))
			Fail("%s with operator %u", name, (uint32)node->op);
		break;
	case NodeType::REFERENCE:
		if (!node->target)
			Fail("REFERENCE without a target");
		break;
	default: break;
	}

	Slice<const ChildRule> rules = ChildRules(node->node_type);
	uint32 counts[16] = {};
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->usage == Usage::ATTRIBUTE)
		{
			if (!CanHaveAttributes(node->node_type))
				Fail("%s has an attribute", name);
			if (!IsExpression(child->node_type))
				Fail("%s attribute is a %s", name, NodeTypeToString(child->node_type).CString());
			continue;
		}
		uint32 i = 0;
		while (i < rules.count && rules[i].usage != child->usage)
			i++;
		if (i == rules.count)
			Fail("%s has a child with usage %s", name, UsageToString(child->usage).CString());
		bool (*allowed)(NodeType) = rules[i].allowed ? rules[i].allowed : IsExpression;
		if (!allowed(child->node_type))
			Fail("%s's %s is a %s", name, UsageToString(child->usage).CString(), NodeTypeToString(child->node_type).CString());
		counts[i]++;
	}
	for (uint32 i = 0; i < rules.count; ++i)
	{
		if (counts[i] < rules[i].min || (rules[i].max != MANY && counts[i] > rules[i].max))
			Fail("%s has %u children with usage %s", name, counts[i], UsageToString(rules[i].usage).CString());
	}
}

// Types

static bool IsPowerOfTwo(uint32 value)
{
	return value && !(value & (value - 1));
}

static void CheckType(Compilation& compilation, Type* type, uint32 depth = 0)
{
	if (depth > 1000)
		Fail("type nested more than 1000 deep");
	if (type->kind != ElementKind::TYPE)
		Fail("type has element kind %u", (uint32)type->kind);
	const char* name = TypeToString(type, compilation.intermediate_arena).CString();
	if (!IsPowerOfTwo(type->alignment))
		Fail("%s has alignment %u", name, type->alignment);
	if (type->size % type->alignment)
		Fail("%s has size %u, not a multiple of its alignment %u", name, type->size, type->alignment);

	switch (type->type_kind)
	{
	case TypeKind::PRIMITIVE:
	case TypeKind::ENUM: break;
	case TypeKind::VECTOR:
	case TypeKind::MATRIX:
	{
		// Only the built-ins exist, made by InitContext.
		break;
	}
	case TypeKind::ARRAY:
	{
		ArrayType* array = (ArrayType*)type;
		Type* element = array->element;
		if (!element)
			Fail("%s has no element type", name);
		CheckType(compilation, element, depth + 1);
		uint64 stride = ((uint64)element->size + element->alignment - 1) / element->alignment * element->alignment;
		if (array->length < 1 || array->stride != stride || (uint64)array->size != stride * array->length ||
			array->alignment != element->alignment)
			Fail("%s has length %u, stride %u, size %u, alignment %u", name, array->length, array->stride, array->size,
				array->alignment);
		if (GetArrayType(compilation.context, element, array->length) != array)
			Fail("%s isn't unique", name);
		break;
	}
	case TypeKind::STRUCT:
	{
		StructType* structure = (StructType*)type;
		if (structure->state != StructState::COMPLETE)
			break; // only typed modules are checked for complete structs
		uint32 members = 0;
		for (Node* field = structure->declaration->child; field; field = field->next)
			members += field->usage == Usage::MEMBER;
		if (structure->fields.count != members)
			Fail("%s has %u fields for %u members", name, structure->fields.count, members);

		uint64 end = 0;
		uint32 alignment = 1;
		for (uint32 i = 0; i < structure->fields.count; ++i)
		{
			StructField& field = structure->fields[i];
			CheckType(compilation, field.type, depth + 1);
			if (field.offset < end || field.offset % field.type->alignment)
				Fail("%s's field %u is at %u, after %llu with alignment %u", name, i, field.offset, (unsigned long long)end,
					field.type->alignment);
			end = (uint64)field.offset + field.type->size;
			if (field.type->alignment > alignment)
				alignment = field.type->alignment;
		}
		if (end > structure->size || structure->alignment != alignment)
			Fail("%s has size %u and alignment %u, its fields end at %llu with alignment %u", name, structure->size,
				structure->alignment, (unsigned long long)end, alignment);
		break;
	}
	default: Fail("type kind %u", (uint32)type->type_kind);
	}
}

// What has to hold for the tree after each stage. A stage that failed leaves a tree that's still well formed, but
// only a successful one guarantees the stage's own invariants.
static void CheckTree(Compilation& compilation, Stage stage, bool succeeded)
{
	struct Entry
	{
		Node* node;
		bool in_attribute;
		bool on_constant; // an attribute of a CONSTANT, ignored for now
	};
	std::vector<Entry> stack = { { compilation.module, false, false } };
	std::unordered_set<Node*> seen;
	uint32 attribute_count = 0;

	if (compilation.module->node_type != NodeType::MODULE || compilation.module->usage != Usage::ROOT)
		Fail("the root is %s", NodeTypeToString(compilation.module->node_type).CString());

	while (!stack.empty())
	{
		Entry entry = stack.back();
		stack.pop_back();
		Node* node = entry.node;
		if (!seen.insert(node).second)
			Fail("%s is in the tree twice", NodeTypeToString(node->node_type).CString());
		if (!compilation.intermediate_arena->Contains(node))
			Fail("%s isn't in the intermediate arena", NodeTypeToString(node->node_type).CString());
		CheckNodeShape(node);
		attribute_count += node->usage == Usage::ATTRIBUTE;
		const char* name = NodeTypeToString(node->node_type).CString();

		if (stage >= Stage::RESOLVE && succeeded)
		{
			if (node->node_type == NodeType::IDENTIFIER)
				Fail("IDENTIFIER %s left after resolving", GetAtomString(node->name, compilation.intermediate_arena).CString());
			if (node->node_type == NodeType::BINARY && node->op == TokenType::COLON)
				Fail("':' left after resolving");
			if ((node->node_type == NodeType::MODULE || node->node_type == NodeType::FUNCTION ||
					node->node_type == NodeType::BLOCK) &&
				!node->scope)
				Fail("%s without a scope", name);
			if (node->node_type == NodeType::MODULE && node->scope->parent != compilation.context.global_scope)
				Fail("the module's scope isn't under the global scope");
			if (node->node_type == NodeType::FUNCTION && FindChild(node, Usage::BODY)->scope != node->scope)
				Fail("FUNCTION %s doesn't share its body's scope", GetAtomString(node->name, compilation.intermediate_arena).CString());
			if (node->node_type == NodeType::REFERENCE && node->target->kind == ElementKind::NODE)
			{
				NodeType target = ((Node*)node->target)->node_type;
				if (target != NodeType::CONST && target != NodeType::FUNCTION && target != NodeType::PARAMETER &&
					target != NodeType::VARIABLE && target != NodeType::ENUM_VALUE)
					Fail("REFERENCE to a %s", NodeTypeToString(target).CString());
			}
		}

		if (stage >= Stage::TYPE && succeeded)
		{
			if (node->type)
				CheckType(compilation, node->type);

			if (node->usage == Usage::ATTRIBUTE && !entry.on_constant)
			{
				// Typing succeeded, so every attribute is one the typer understood.
				Node* callee = node->node_type == NodeType::CALL ? FindChild(node, Usage::CALLEE) : node;
				if (callee->node_type != NodeType::REFERENCE || callee->target->kind != ElementKind::INTRINSIC)
					Fail("attribute %s on a %s isn't an intrinsic", NodeTypeToString(callee->node_type).CString(),
						NodeTypeToString(node->node_type).CString());
			}
			else if (!entry.in_attribute)
			{
				bool untyped = node->node_type == NodeType::MODULE || node->node_type == NodeType::STRUCT ||
							   node->node_type == NodeType::FUNCTION || node->node_type == NodeType::BLOCK ||
							   node->node_type == NodeType::RETURN;
				if (!untyped && !node->type)
					Fail("%s %s has no type", name, UsageToString(node->usage).CString());
			}

			// Constant expressions are folded into CONSTANTs.
			if (node->node_type == NodeType::CONST && FindChild(node, Usage::VALUE)->node_type != NodeType::CONSTANT)
				Fail("CONST's value isn't a CONSTANT");
			if (node->node_type == NodeType::ARRAY_TYPE && FindChild(node, Usage::SIZE)->node_type != NodeType::CONSTANT)
				Fail("array size isn't a CONSTANT");
			if (node->node_type == NodeType::CONSTANT)
			{
				Constant* constant = node->constant;
				if (!constant || constant->kind != ElementKind::CONSTANT)
					Fail("CONSTANT without a constant");
				if (constant->type != node->type || constant->bytes.count != node->type->size ||
					(constant->bytes.count && !constant->bytes.data))
					Fail("CONSTANT of type %s has a %s of %u bytes", TypeToString(node->type, compilation.intermediate_arena).CString(),
						TypeToString(constant->type, compilation.intermediate_arena).CString(), constant->bytes.count);
			}
		}

		for (Node* child = node->child; child; child = child->next)
		{
			stack.push_back({ child, entry.in_attribute || child->usage == Usage::ATTRIBUTE,
				entry.on_constant || node->node_type == NodeType::CONSTANT });
		}
	}

	// Every '@' parses into one attribute, which no stage may drop.
	if ((stage > Stage::PARSE || succeeded) && attribute_count != compilation.attribute_count)
		Fail("%u attributes in the tree for %u in the source", attribute_count, compilation.attribute_count);
}

// Shader interface

// The entry stage of a function, if it has one entry attribute. The typer checked the argument.
static bool EntryStage(Node* function, ShaderStage* stage)
{
	uint32 count = 0;
	for (Node* attribute = function->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE || attribute->node_type != NodeType::CALL)
			continue;
		Node* callee = FindChild(attribute, Usage::CALLEE);
		if (callee->node_type != NodeType::REFERENCE || callee->target->kind != ElementKind::INTRINSIC ||
			((Intrinsic*)callee->target)->intrinsic_kind != IntrinsicKind::ENTRY)
			continue;
		*stage = (ShaderStage)((Node*)FindChild(attribute, Usage::ARGUMENT)->target)->enum_value;
		count++;
	}
	return count == 1;
}

// The semantic and location attributes of a declaration, read directly rather than through the pass. The last one's
// kind and value go to kind and value.
static uint32 ReadIOAttributes(Node* declaration, IOKind* kind, uint32* value)
{
	uint32 count = 0;
	for (Node* attribute = declaration->child; attribute; attribute = attribute->next)
	{
		if (attribute->usage != Usage::ATTRIBUTE || attribute->node_type != NodeType::CALL)
			continue;
		Node* callee = FindChild(attribute, Usage::CALLEE);
		if (callee->node_type != NodeType::REFERENCE || callee->target->kind != ElementKind::INTRINSIC)
			continue;
		IntrinsicKind intrinsic = ((Intrinsic*)callee->target)->intrinsic_kind;
		Node* argument = FindChild(attribute, Usage::ARGUMENT);
		if (intrinsic == IntrinsicKind::SEMANTIC)
		{
			*kind = IOKind::SEMANTIC;
			*value = (uint32)((Node*)argument->target)->enum_value;
		}
		else if (intrinsic == IntrinsicKind::LOCATION)
		{
			if (argument->node_type != NodeType::CONSTANT)
				Fail("location's argument isn't a CONSTANT");
			*kind = IOKind::LOCATION;
			*value = (uint32)ConstantToInteger(argument->constant);
		}
		else
			continue;
		count++;
	}
	return count;
}

// Scalar and vector leaves of a value of type. Only called on interfaces that were built, which have few leaves.
static uint32 CountLeaves(Type* type, uint32 depth = 0)
{
	if (depth > 1000)
		Fail("shader IO nested more than 1000 deep");
	if (type->type_kind != TypeKind::STRUCT)
		return 1;
	StructType* structure = (StructType*)type;
	uint32 count = 0;
	for (uint32 i = 0; i < structure->fields.count; ++i)
		count += CountLeaves(structure->fields[i].type, depth + 1);
	return count;
}

static bool IsVoidType(Type* type)
{
	return type->type_kind == TypeKind::PRIMITIVE && ((PrimitiveType*)type)->primitive_kind == PrimitiveKind::VOID;
}

static void CheckShaderIO(Compilation& compilation, EntryPoint& entry_point, ShaderIO& io)
{
	Arena* arena = compilation.intermediate_arena;
	const char* function_name = GetAtomString(entry_point.function->name, arena).CString();
	if (io.path.count && !arena->Contains(io.path.data))
		Fail("%s's IO path isn't in the intermediate arena", function_name);

	// The path leads to the declaration and the leaf type.
	Node* declaration = nullptr;
	uint32 i = 0;
	if (io.direction == IODirection::INPUT)
	{
		if (!io.path.count)
			Fail("%s has an input with an empty path", function_name);
		uint32 index = 0;
		for (Node* child = entry_point.function->child; child; child = child->next)
		{
			if (child->usage == Usage::PARAMETER && index++ == io.path[0])
				declaration = child;
		}
		if (!declaration)
			Fail("%s's input path starts at parameter %u, it has %u", function_name, io.path[0], index);
		i = 1;
	}
	else
	{
		declaration = FindChild(entry_point.function, Usage::RETURN_TYPE);
		if (!declaration)
			Fail("%s has an output without a return type", function_name);
	}
	Type* type = declaration->type;
	for (; i < io.path.count; ++i)
	{
		if (type->type_kind != TypeKind::STRUCT || io.path[i] >= ((StructType*)type)->fields.count)
			Fail("%s's IO path goes through %s at %u", function_name, TypeToString(type, arena).CString(), i);
		StructField& field = ((StructType*)type)->fields[io.path[i]];
		declaration = field.declaration;
		type = field.type;
	}
	if (type != io.type || declaration != io.declaration)
		Fail("%s's IO path leads to %s, the record has %s", function_name, TypeToString(type, arena).CString(),
			TypeToString(io.type, arena).CString());

	PrimitiveType* component = ComponentType(type);
	if (!component || !IsNumeric(component))
		Fail("%s has an IO of type %s", function_name, TypeToString(type, arena).CString());

	IOKind kind = IOKind::SEMANTIC;
	uint32 value = 0;
	if (ReadIOAttributes(declaration, &kind, &value) != 1)
		Fail("%s has an IO whose declaration doesn't have exactly one semantic or location", function_name);
	uint32 recorded = io.io_kind == IOKind::SEMANTIC ? (uint32)io.semantic : io.location;
	if (kind != io.io_kind || value != recorded)
		Fail("%s has an IO recorded as %u %u, declared as %u %u", function_name, (uint32)io.io_kind, recorded, (uint32)kind, value);

	bool color_target = entry_point.stage == ShaderStage::FRAGMENT && io.direction == IODirection::OUTPUT;
	if (io.io_kind == IOKind::LOCATION && io.location >= (color_target ? 8u : 32u))
		Fail("%s has location %u", function_name, io.location);
	if (io.io_kind == IOKind::SEMANTIC)
	{
		bool vertex = entry_point.stage == ShaderStage::VERTEX;
		bool input = io.direction == IODirection::INPUT;
		ZTStringView type_name = TypeToString(type, arena);
		bool valid = false;
		if (io.semantic == Semantic::VERTEX_INDEX)
			valid = vertex && input && type_name == "uint";
		else if (io.semantic == Semantic::POSITION)
			valid = vertex != input && type_name == "float4";
		if (!valid)
			Fail("%s has semantic %s as an %s of type %s", function_name, SemanticToString(io.semantic).CString(),
				input ? "input" : "output", type_name.CString());
	}
}

// Whether path a comes before b, in the order the parameters and fields are declared.
static bool PathBefore(Slice<uint32> a, Slice<uint32> b)
{
	for (uint32 i = 0; i < a.count && i < b.count; ++i)
	{
		if (a[i] != b[i])
			return a[i] < b[i];
	}
	return a.count < b.count;
}

static void CheckShaderInterface(Compilation& compilation, bool succeeded)
{
	ShaderInterface& shader_interface = compilation.shader_interface;
	Arena* arena = compilation.intermediate_arena;
	if (shader_interface.entry_points.count && !arena->Contains(shader_interface.entry_points.data))
		Fail("the entry points aren't in the intermediate arena");
	if (!succeeded)
		return;

	// The entry points are the top-level functions with an entry attribute, in order.
	uint32 index = 0;
	for (Node* function = compilation.module->child; function; function = function->next)
	{
		ShaderStage stage = ShaderStage::VERTEX;
		if (function->node_type != NodeType::FUNCTION || !EntryStage(function, &stage))
			continue;
		if (index >= shader_interface.entry_points.count)
			Fail("'%s' is missing from the entry points", GetAtomString(function->name, arena).CString());
		EntryPoint& entry_point = shader_interface.entry_points[index++];
		if (entry_point.function != function || entry_point.stage != stage)
			Fail("entry point %u is '%s', expected '%s'", index - 1, GetAtomString(entry_point.function->name, arena).CString(),
				GetAtomString(function->name, arena).CString());
	}
	if (index != shader_interface.entry_points.count)
		Fail("%u entry points for %u entry functions", shader_interface.entry_points.count, index);

	for (uint32 e = 0; e < shader_interface.entry_points.count; ++e)
	{
		EntryPoint& entry_point = shader_interface.entry_points[e];
		const char* function_name = GetAtomString(entry_point.function->name, arena).CString();
		if (entry_point.io.count && !arena->Contains(entry_point.io.data))
			Fail("%s's IO isn't in the intermediate arena", function_name);

		// Every leaf of the parameters and return value is an input or output.
		uint32 leaves = 0;
		for (Node* parameter = entry_point.function->child; parameter; parameter = parameter->next)
		{
			if (parameter->usage == Usage::PARAMETER)
				leaves += CountLeaves(parameter->type);
		}
		Node* return_node = FindChild(entry_point.function, Usage::RETURN_TYPE);
		if (return_node && !IsVoidType(return_node->type))
			leaves += CountLeaves(return_node->type);
		if (entry_point.io.count != leaves)
			Fail("%s has %u IO records for %u leaves", function_name, entry_point.io.count, leaves);

		uint32 locations[2] = {};
		uint32 semantics[2] = {};
		for (uint32 i = 0; i < entry_point.io.count; ++i)
		{
			ShaderIO& io = entry_point.io[i];
			CheckShaderIO(compilation, entry_point, io);
			if (i)
			{
				ShaderIO& previous = entry_point.io[i - 1];
				if (previous.direction > io.direction ||
					(previous.direction == io.direction && !PathBefore(previous.path, io.path)))
					Fail("%s's IO records are out of order at %u", function_name, i);
			}
			uint32 direction = (uint32)io.direction;
			uint32* used = io.io_kind == IOKind::SEMANTIC ? &semantics[direction] : &locations[direction];
			uint32 bit = 1u << (io.io_kind == IOKind::SEMANTIC ? (uint32)io.semantic : io.location);
			if (*used & bit)
				Fail("%s uses an IO twice", function_name);
			*used |= bit;
		}
		if (entry_point.stage == ShaderStage::VERTEX && !(semantics[(uint32)IODirection::OUTPUT] & (1u << (uint32)Semantic::POSITION)))
			Fail("vertex shader %s doesn't output position", function_name);
	}
}

static uint32 CountAttributes(ZTStringView source)
{
	Arena* arena = CreateArena();
	DEFER(DestroyArena(arena));
	Parser parser = { .source = (char*)source.CString(), .head = (char*)source.CString(), .arena = arena, .error_arena = arena };
	uint32 count = 0;
	while (LexToken(parser) && parser.token.token_type != TokenType::END_OF_FILE)
	{
		count += parser.token.token_type == TokenType::AT;
		EatToken(parser);
	}
	return count;
}

static bool BuildInterface(Compilation& compilation);

// Emits each entry point for both targets, checking the output with the targets' tools if validate.
static void EmitBackends(Compilation& compilation, bool validate)
{
	IRModule& ir = compilation.ir;
	Arena* arena = compilation.intermediate_arena;
	for (IRRef function = ir.first_function; function; function = ir[function].function.info->next)
	{
		EntryPoint* entry_point = ir[function].function.info->entry_point;
		if (!entry_point)
			continue;
		Slice<uint32> words = EmitSPIRV(ir, function, arena);
		ZTStringView hlsl = EmitHLSL(ir, function, arena);
		compilation.spirv.push_back(words);
		compilation.hlsl.push_back(hlsl);
		if (!validate)
			continue;
		ZTStringView problem = Validation::ValidateSPIRV(words, arena);
		if (problem.length)
			Fail("invalid SPIR-V: %s\n%s", problem.CString(), Validation::DisassembleSPIRV(words, arena).CString());
		problem = Validation::CompileHLSL(hlsl, entry_point->stage, arena);
		if (problem.length)
			Fail("invalid HLSL: %s\n%s", problem.CString(), hlsl.CString());
	}
}

static void CompileStages(Compilation& compilation, ZTStringView source, ContextKind kind, uint8 fill, bool validate)
{
	SetArenaFill(fill);
	DEFER(SetArenaFill(-1));
	compilation.output_arena = CreateArena();
	compilation.intermediate_arena = CreateArena();
	InitContext(compilation.context, compilation.intermediate_arena, kind);

	Parser parser = {
		.source = (char*)source.CString(),
		.head = (char*)source.CString(),
		.arena = compilation.intermediate_arena,
		.error_arena = compilation.output_arena,
	};
	bool parsed = Parse(parser, &compilation.module);
	if (parsed)
		compilation.attribute_count = CountAttributes(source);
	CheckErrors(compilation, parser.errors, parsed, "parsing");
	if (!compilation.module)
		Fail("Parse returned no module");
	CheckTree(compilation, Stage::PARSE, parsed);
	if (!parsed)
	{
		compilation.failed_stage = Stage::PARSE;
		compilation.errors = CopyErrors(compilation.output_arena, parser.errors);
		return;
	}

	Resolver resolver = {
		.context = &compilation.context,
		.arena = compilation.intermediate_arena,
		.error_arena = compilation.output_arena,
	};
	bool resolved = Resolve(resolver, compilation.module);
	CheckErrors(compilation, resolver.errors, resolved, "resolving");
	CheckTree(compilation, Stage::RESOLVE, resolved);
	if (!resolved)
	{
		compilation.failed_stage = Stage::RESOLVE;
		compilation.errors = CopyErrors(compilation.output_arena, resolver.errors);
		return;
	}

	Typer typer = {
		.context = &compilation.context,
		.arena = compilation.intermediate_arena,
		.error_arena = compilation.output_arena,
	};
	bool typed = TypeCheck(typer, compilation.module);
	CheckErrors(compilation, typer.errors, typed, "typing");
	CheckTree(compilation, Stage::TYPE, typed);
	if (!typed)
	{
		compilation.failed_stage = Stage::TYPE;
		compilation.errors = CopyErrors(compilation.output_arena, typer.errors);
		return;
	}
	if (kind == ContextKind::SHADER)
	{
		if (!BuildInterface(compilation))
			return;
	}

	// Nothing fails past the front end, but the IR has to be valid.
	InitIRModule(compilation.ir, &compilation.context, compilation.intermediate_arena);
	GenerateIR(compilation.ir, compilation.module, kind == ContextKind::SHADER ? &compilation.shader_interface : nullptr);
	ZTStringView error = ValidateIR(compilation.ir, compilation.intermediate_arena);
	if (error.length)
		Fail("IR gen made invalid IR: %s\n%s", error.CString(), IRModuleToString(compilation.ir, compilation.intermediate_arena).CString());
	if (kind != ContextKind::SHADER)
		return;

	ClampIndices(compilation.ir);
	error = ValidateIR(compilation.ir, compilation.intermediate_arena);
	if (error.length)
		Fail("clamping indices made invalid IR: %s\n%s", error.CString(),
			IRModuleToString(compilation.ir, compilation.intermediate_arena).CString());
	EmitBackends(compilation, validate);
}

static bool BuildInterface(Compilation& compilation)
{
	ShaderInterfaceBuilder builder = { .arena = compilation.intermediate_arena, .error_arena = compilation.output_arena };
	bool built = BuildShaderInterface(builder, compilation.module, &compilation.shader_interface);
	CheckErrors(compilation, builder.errors, built, "building the shader interface");
	CheckShaderInterface(compilation, built);
	if (!built)
	{
		compilation.failed_stage = Stage::INTERFACE;
		compilation.errors = CopyErrors(compilation.output_arena, builder.errors);
	}
	return built;
}

// Allowed arena memory for one compile: enough for TOTAL_CONSTANT_SIZE_LIMIT and the tree of the source, so anything
// past it means a small input made the compiler use disproportionate memory.
static const size_t MEMORY_LIMIT = (size_t)64 * 1024 * 1024;
static const size_t MEMORY_LIMIT_PER_SOURCE_BYTE = 1024;

void Compile(Compilation& compilation, ZTStringView source, ContextKind kind, uint8 fill, bool validate)
{
	CompileStages(compilation, source, kind, fill, validate);
	size_t memory = compilation.intermediate_arena->Size() + compilation.output_arena->Size();
	if (memory > MEMORY_LIMIT + MEMORY_LIMIT_PER_SOURCE_BYTE * source.length)
		Fail("compiling %zu bytes of source used %zu bytes of arenas", source.length, memory);
}

ZTStringView Fingerprint(Compilation& compilation, Arena* arena)
{
	StringBuilder builder(arena);
	builder.AppendFormat("failed stage %u\n", (uint32)compilation.failed_stage);
	for (uint32 i = 0; i < compilation.errors.count; ++i)
	{
		builder.Append(compilation.errors[i]->message);
		builder.Append("\n");
	}
	// Past resolving the tree is at most RECURSION_LIMIT deep, so serializing it recursively is safe.
	if (compilation.failed_stage >= Stage::TYPE)
		SerializeNode(builder, compilation.module);
	if (compilation.failed_stage >= Stage::INTERFACE)
	{
		builder.Append("\n");
		builder.Append(ShaderInterfaceToString(compilation.shader_interface, arena));
	}
	if (compilation.failed_stage == Stage::DONE)
		builder.Append(IRModuleToString(compilation.ir, arena));
	for (size_t i = 0; i < compilation.hlsl.size(); ++i)
	{
		builder.Append(compilation.hlsl[i]);
		Slice<uint32> words = compilation.spirv[i];
		for (uint32 w = 0; w < words.count; ++w)
			builder.AppendFormat("%08x%s", words[w], w % 8 == 7 ? "\n" : " ");
		builder.Append("\n");
	}
	return builder.ToString();
}

void Destroy(Compilation& compilation)
{
	// Poisoned under ASan, so errors pointing into it are caught below.
	DestroyArena(compilation.intermediate_arena);
	compilation.intermediate_arena = nullptr;
	for (uint32 i = 0; i < compilation.errors.count; ++i)
	{
		ZTStringView message = compilation.errors[i]->message;
		if (strlen(message.CString()) != message.length)
			Fail("error message changed after the intermediate arena was destroyed");
	}
	DestroyArena(compilation.output_arena);
	compilation.output_arena = nullptr;
}

void CheckSource(ZTStringView source, void (*check)(Compilation& compilation, void* user), void* user)
{
	current_source = source;
	DEFER(current_source = {});

	Arena* scratch = CreateArena();
	DEFER(DestroyArena(scratch));

	// Different garbage in the arenas has to give the same result.
	Compilation first;
	Compile(first, source, ContextKind::SHADER, 0x00, true);
	if (check)
		check(first, user);
	Compilation second;
	Compile(second, source, ContextKind::SHADER, 0xA5);
	ZTStringView first_fingerprint = Fingerprint(first, scratch);
	ZTStringView second_fingerprint = Fingerprint(second, scratch);
	if (!(first_fingerprint == second_fingerprint))
		Fail("results differ with different arena contents, uninitialized memory?\n---- zeroed ----\n%s\n---- filled ----\n%s",
			first_fingerprint.CString(), second_fingerprint.CString());
	Destroy(first);
	Destroy(second);

	// Scripts have a different global scope.
	Compilation script;
	Compile(script, source, ContextKind::SCRIPT, 0x00);
	Destroy(script);
}

}
