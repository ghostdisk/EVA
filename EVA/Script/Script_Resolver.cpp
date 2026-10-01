#include <EVA/Script/Script.hpp>
#include <stdarg.h>

namespace EVA::Script
{

ScriptError* EmitError(Resolver& resolver, const char* format, ...)
{
	ScriptError* error = resolver.arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(resolver.arena, format, args);
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

static Node* FindInScope(Scope* scope, Atom name)
{
	for (Definition* definition = scope->first; definition; definition = definition->next)
	{
		if (definition->name == name)
			return definition->node;
	}
	return nullptr;
}

// The declaration the name refers to from the current scope, or nullptr.
static Node* Lookup(Resolver& resolver, Atom name)
{
	for (Scope* scope = resolver.scope; scope; scope = scope->parent)
	{
		if (Node* node = FindInScope(scope, name))
			return node;
	}
	return nullptr;
}

// Adds the node to the current scope under its name. Names from parent scopes can be shadowed, not ones from this scope.
static bool Declare(Resolver& resolver, Node* node)
{
	if (FindInScope(resolver.scope, node->name))
	{
		EmitError(resolver, "'%s' is already defined", GetAtomString(node->name, resolver.arena).CString());
		return false;
	}

	Definition* definition = resolver.arena->New<Definition>();
	definition->name = node->name;
	definition->node = node;
	definition->next = resolver.scope->first;
	resolver.scope->first = definition;
	return true;
}

// First pass over a scope's statements or declarations, so functions and structs can be used before they're declared.
static bool DeclareAhead(Resolver& resolver, Node* node)
{
	bool resolved = true;
	for (Node* child = node->child; child; child = child->next)
	{
		if (child->type == NodeType::FUNCTION || child->type == NodeType::STRUCT)
			resolved = Declare(resolver, child) && resolved;
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
	if (left->type != NodeType::IDENTIFIER)
	{
		EmitError(resolver, "expected a name before ':'");
		ResolveChildren(resolver, node);
		return false;
	}

	Node** link = &node->child;
	while (*link != left)
		link = &(*link)->next;
	*link = left->next;

	node->type = NodeType::VARIABLE;
	node->name = left->name;
	node->text = nullptr; // clears the op
	right->usage = Usage::TYPE;

	bool resolved = ResolveChildren(resolver, node);
	return Declare(resolver, node) && resolved;
}

static bool ResolveNode(Resolver& resolver, Node* node)
{
	CHECK_RECURSION(resolver);
	Scope* outer = resolver.scope;
	DEFER(resolver.scope = outer);

	switch (node->type)
	{
	case NodeType::MODULE:
	{
		// The module's names live in its own scope. Its parent, the global scope, will hold the built-ins.
		node->scope = NewScope(resolver, NewScope(resolver, nullptr));
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
		return Declare(resolver, node) && resolved;
	}
	case NodeType::BINARY:
	{
		if (node->op == TokenType::COLON)
			return ResolveVariable(resolver, node);
		return ResolveChildren(resolver, node);
	}
	case NodeType::IDENTIFIER:
	{
		bool resolved = true;
		if (Node* target = Lookup(resolver, node->name))
		{
			node->type = NodeType::REFERENCE;
			node->target = target;
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
