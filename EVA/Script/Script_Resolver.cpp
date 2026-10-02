#include <EVA/Script/Script.hpp>
#include <stdarg.h>

namespace EVA::Script
{

ScriptError* EmitError(Resolver& resolver, const char* format, ...)
{
	ScriptError* error = resolver.error_arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(resolver.error_arena, format, args);
	va_end(args);
	resolver.errors.push_back(error);
	return error;
}

static Scope* NewScope(Resolver& resolver, Scope* parent)
{
	Scope* scope = resolver.arena->New<Scope>();
	scope->parent = parent;
	return scope;
}

static Definition* FindInScope(Scope* scope, Atom name)
{
	for (Definition* definition = scope->first; definition; definition = definition->next)
	{
		if (definition->name == name)
			return definition;
	}
	return nullptr;
}

// What the name refers to from the current scope, or nullptr.
static Definition* Lookup(Resolver& resolver, Atom name)
{
	for (Scope* scope = resolver.scope; scope; scope = scope->parent)
	{
		if (Definition* definition = FindInScope(scope, name))
			return definition;
	}
	return nullptr;
}

// Adds the element to the current scope under name. Names from parent scopes can be shadowed, not ones from this scope.
static bool Declare(Resolver& resolver, Atom name, Element* element)
{
	if (FindInScope(resolver.scope, name))
	{
		EmitError(resolver, "'%s' is already defined", GetAtomString(name, resolver.arena).CString());
		return false;
	}

	Definition* definition = resolver.arena->New<Definition>();
	definition->name = name;
	definition->element = element;
	definition->next = resolver.scope->first;
	resolver.scope->first = definition;
	return true;
}

// A struct names its StructType, so references to it are references to a type.
static StructType* NewStructType(Resolver& resolver, Node* node)
{
	StructType* type = resolver.arena->New<StructType>();
	type->name = node->name;
	type->declaration = node;
	node->type = type;
	return type;
}

// First pass over a scope's statements or declarations, so functions and structs can be used before they're declared.
static bool DeclareAhead(Resolver& resolver, Node* node)
{
	bool resolved = true;
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->node_type == NodeType::FUNCTION)
			resolved = Declare(resolver, child->name, child) && resolved;
		else if (child->node_type == NodeType::STRUCT)
			resolved = Declare(resolver, child->name, NewStructType(resolver, child)) && resolved;
	}
	return resolved;
}

static bool ResolveNode(Resolver& resolver, Node* node);

static bool ResolveChildren(Resolver& resolver, Node* node)
{
	bool resolved = true;
	for (Node* child = node->child; child; child = child->next)
		resolved = ResolveNode(resolver, child) && resolved;
	return resolved;
}

// name: type declares a variable. The node is reused: the name moves onto it from the identifier, which is dropped, and
// the type stays as its child. Declared after the type is resolved, so the type can't refer to it.
static bool ResolveVariable(Resolver& resolver, Node* node)
{
	Node* left = FindChild(node, Usage::LEFT);
	Node* right = FindChild(node, Usage::RIGHT);
	if (left->node_type != NodeType::IDENTIFIER)
	{
		EmitError(resolver, "expected a name before ':'");
		ResolveChildren(resolver, node);
		return false;
	}

	// The name's attributes, e.g. (@a x): int, take its place.
	Node* replacement = nullptr;
	Node** tail = &replacement;
	for (Node* child = left->child; child; child = child->next)
	{
		if (child->usage == Usage::ATTRIBUTE)
		{
			*tail = child;
			tail = &child->next;
		}
	}
	*tail = left->next;

	Node** link = &node->child;
	while (*link != left)
	{
		assert(*link);
		link = &(*link)->next;
	}
	*link = replacement;

	node->node_type = NodeType::VARIABLE;
	node->name = left->name;
	node->number = nullptr; // clears the op
	right->usage = Usage::DECLARED_TYPE;

	bool resolved = ResolveChildren(resolver, node);
	return Declare(resolver, node->name, node) && resolved;
}

static bool ResolveNode(Resolver& resolver, Node* node)
{
	CHECK_RECURSION(resolver);
	Scope* outer = resolver.scope;
	DEFER(resolver.scope = outer);

	switch (node->node_type)
	{
	case NodeType::MODULE:
	{
		// The module's names live in its own scope, under the global scope holding the built-ins.
		node->scope = NewScope(resolver, resolver.context->global_scope);
		resolver.scope = node->scope;
		bool resolved = DeclareAhead(resolver, node);
		return ResolveChildren(resolver, node) && resolved;
	}
	case NodeType::FUNCTION:
	{
		// Parameters and the return type are resolved in the function's scope, which its body shares.
		node->scope = NewScope(resolver, resolver.scope);
		resolver.scope = node->scope;
		if (Node* body = FindChild(node, Usage::BODY))
			body->scope = node->scope;
		return ResolveChildren(resolver, node);
	}
	case NodeType::BLOCK:
	{
		if (!node->scope)
			node->scope = NewScope(resolver, resolver.scope);
		resolver.scope = node->scope;
		bool resolved = DeclareAhead(resolver, node);
		return ResolveChildren(resolver, node) && resolved;
	}
	case NodeType::CONST:
	case NodeType::PARAMETER:
	{
		// Declared after their type and value, so those can't refer to them.
		bool resolved = ResolveChildren(resolver, node);
		return Declare(resolver, node->name, node) && resolved;
	}
	case NodeType::BINARY:
	{
		if (node->op == TokenType::COLON)
			return ResolveVariable(resolver, node);
		return ResolveChildren(resolver, node);
	}
	case NodeType::CALL:
	{
		bool resolved = true;
		for (Node* child = node->child; child; child = child->next)
		{
			if (child->usage != Usage::ARGUMENT)
				resolved = ResolveNode(resolver, child) && resolved;
		}

		// A fresh scope, so declarations in the arguments don't end up in the intrinsic's, which outlives the module.
		Node* callee = FindChild(node, Usage::CALLEE);
		if (callee->node_type == NodeType::REFERENCE && callee->target->kind == ElementKind::INTRINSIC)
		{
			if (Scope* argument_scope = ((Intrinsic*)callee->target)->argument_scope)
				resolver.scope = NewScope(resolver, argument_scope);
		}

		for (Node* child = node->child; child; child = child->next)
		{
			if (child->usage == Usage::ARGUMENT)
				resolved = ResolveNode(resolver, child) && resolved;
		}
		return resolved;
	}
	case NodeType::IDENTIFIER:
	{
		bool resolved = true;
		if (Definition* definition = Lookup(resolver, node->name))
		{
			node->node_type = NodeType::REFERENCE;
			node->target = definition->element;
		}
		else
		{
			EmitError(resolver, "unknown identifier '%s'", GetAtomString(node->name, resolver.arena).CString());
			resolved = false;
		}
		return ResolveChildren(resolver, node) && resolved; // attributes
	}
	default:
		// STRUCT names were declared ahead. MEMBER's name depends on the object's type, so only its object is resolved.
		return ResolveChildren(resolver, node);
	}
}

bool Resolve(Resolver& resolver, Node* module)
{
	ResolveNode(resolver, module);
	return resolver.errors.empty();
}

}
