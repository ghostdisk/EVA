#include <EVA/Script/Script.hpp>
#include <stdio.h>

namespace EVA::Script
{

ZTStringView TokenToString(TokenType token_type)
{
	switch (token_type)
	{
		case TokenType::END_OF_FILE: return "end of file";

		case TokenType::SEMICOLON: return ";";
		case TokenType::COMMA: return ",";
		case TokenType::PLUS: return "+";
		case TokenType::MINUS: return "-";
		case TokenType::EQUALS: return "=";
		case TokenType::ASTERISK: return "*";
		case TokenType::SLASH: return "/";
		case TokenType::PERCENT: return "%";
		case TokenType::AMPERSAND: return "&";
		case TokenType::PIPE: return "|";
		case TokenType::CARET: return "^";
		case TokenType::TILDE: return "~";
		case TokenType::EXCLAMATION: return "!";
		case TokenType::COLON: return ":";
		case TokenType::DOT: return ".";
		case TokenType::LESS: return "<";
		case TokenType::GREATER: return ">";
		case TokenType::LEFT_PAREN: return "(";
		case TokenType::RIGHT_PAREN: return ")";
		case TokenType::LEFT_BRACKET: return "[";
		case TokenType::RIGHT_BRACKET: return "]";
		case TokenType::LEFT_BRACE: return "{";
		case TokenType::RIGHT_BRACE: return "}";
		case TokenType::AT: return "@";

		case TokenType::IDENTIFIER: return "identifier";
		case TokenType::NUMBER: return "number";

		case TokenType::ADD_ASSIGN: return "+=";
		case TokenType::SUBTRACT_ASSIGN: return "-=";
		case TokenType::MULTIPLY_ASSIGN: return "*=";
		case TokenType::DIVIDE_ASSIGN: return "/=";
		case TokenType::MODULO_ASSIGN: return "%=";
		case TokenType::BIT_AND_ASSIGN: return "&=";
		case TokenType::BIT_OR_ASSIGN: return "|=";
		case TokenType::BIT_XOR_ASSIGN: return "^=";
		case TokenType::SHIFT_LEFT_ASSIGN: return "<<=";
		case TokenType::SHIFT_RIGHT_ASSIGN: return ">>=";
		case TokenType::INCREMENT: return "++";
		case TokenType::DECREMENT: return "--";
		case TokenType::SHIFT_LEFT: return "<<";
		case TokenType::SHIFT_RIGHT: return ">>";
		case TokenType::EQUAL: return "==";
		case TokenType::NOT_EQUAL: return "!=";
		case TokenType::LESS_EQUAL: return "<=";
		case TokenType::GREATER_EQUAL: return ">=";
		case TokenType::LOGICAL_AND: return "&&";
		case TokenType::LOGICAL_OR: return "||";

		case TokenType::KW_CONST: return "const";
		case TokenType::KW_STRUCT: return "struct";
		case TokenType::KW_FUNCTION: return "function";
		case TokenType::KW_IF: return "if";
		case TokenType::KW_ELSE: return "else";
		case TokenType::KW_RETURN: return "return";
		case TokenType::KW_TRUE: return "true";
		case TokenType::KW_FALSE: return "false";
	}
	return "?";
}

ZTStringView NodeTypeToString(NodeType type)
{
	switch (type)
	{
		case NodeType::NONE: return "NONE";
		case NodeType::MODULE: return "MODULE";
		case NodeType::CONST: return "CONST";
		case NodeType::STRUCT: return "STRUCT";
		case NodeType::FUNCTION: return "FUNCTION";
		case NodeType::PARAMETER: return "PARAMETER";
		case NodeType::FIELD: return "FIELD";
		case NodeType::VARIABLE: return "VARIABLE";
		case NodeType::ENUM_VALUE: return "ENUM_VALUE";
		case NodeType::BLOCK: return "BLOCK";
		case NodeType::RETURN: return "RETURN";
		case NodeType::NUMBER: return "NUMBER";
		case NodeType::BOOL: return "BOOL";
		case NodeType::IDENTIFIER: return "IDENTIFIER";
		case NodeType::REFERENCE: return "REFERENCE";
		case NodeType::INIT_LIST: return "INIT_LIST";
		case NodeType::UNARY: return "UNARY";
		case NodeType::POSTFIX: return "POSTFIX";
		case NodeType::BINARY: return "BINARY";
		case NodeType::CALL: return "CALL";
		case NodeType::MEMBER: return "MEMBER";
		case NodeType::INDEX: return "INDEX";
		case NodeType::ARRAY_TYPE: return "ARRAY_TYPE";
		case NodeType::IF: return "IF";
	}
	return "?";
}

ZTStringView UsageToString(Usage usage)
{
	switch (usage)
	{
		case Usage::NONE: return "NONE";
		case Usage::ROOT: return "ROOT";
		case Usage::DECLARATION: return "DECLARATION";
		case Usage::ATTRIBUTE: return "ATTRIBUTE";
		case Usage::VALUE: return "VALUE";
		case Usage::MEMBER: return "MEMBER";
		case Usage::PARAMETER: return "PARAMETER";
		case Usage::RETURN_TYPE: return "RETURN_TYPE";
		case Usage::BODY: return "BODY";
		case Usage::TYPE: return "TYPE";
		case Usage::STATEMENT: return "STATEMENT";
		case Usage::ELEMENT: return "ELEMENT";
		case Usage::SIZE: return "SIZE";
		case Usage::OPERAND: return "OPERAND";
		case Usage::LEFT: return "LEFT";
		case Usage::RIGHT: return "RIGHT";
		case Usage::CALLEE: return "CALLEE";
		case Usage::ARGUMENT: return "ARGUMENT";
		case Usage::OBJECT: return "OBJECT";
		case Usage::INDEX: return "INDEX";
		case Usage::CONDITION: return "CONDITION";
		case Usage::THEN: return "THEN";
		case Usage::ELSE: return "ELSE";
	}
	return "?";
}

// A node's type, or a built-in's kind and name, e.g. TYPE float4. A constant is named by its type.
static ZTStringView TargetToString(Element* target, Arena* arena)
{
	switch (target->kind)
	{
		case ElementKind::NODE: return NodeTypeToString(((Node*)target)->node_type);
		case ElementKind::TYPE: return aprintf(arena, "TYPE %s", GetAtomString(((Type*)target)->name, arena).CString());
		case ElementKind::INTRINSIC: return aprintf(arena, "INTRINSIC %s", GetAtomString(((Intrinsic*)target)->name, arena).CString());
		case ElementKind::CONSTANT: return aprintf(arena, "CONSTANT %s", GetAtomString(((Constant*)target)->type->name, arena).CString());
		case ElementKind::NONE: break;
	}
	return "?";
}

// Prints node and its subtree, one node per line. The node's own line is started by the caller (indent and usage)
// and ended by the caller (newline), so root calls print no usage. The arena is only used for atom strings and is rewound afterwards.
void DumpNode(Node* node, Arena* arena, int indent)
{
	uint8* mark = arena->head;

	printf("\x1b[33m%s\x1b[0m", NodeTypeToString(node->node_type).CString());
	if (node->name != Atom::NONE)
		printf(" | %s", GetAtomString(node->name, arena).CString());

	switch (node->node_type)
	{
		case NodeType::NUMBER:
			printf(" | %s", node->text);
			break;
		case NodeType::BOOL:
			printf(" | %s", node->value ? "true" : "false");
			break;
		case NodeType::ENUM_VALUE:
			printf(" | %lld", (long long)node->enum_value);
			break;
		case NodeType::UNARY:
		case NodeType::POSTFIX:
		case NodeType::BINARY:
			printf(" | %s", TokenToString(node->op).CString());
			break;
		case NodeType::REFERENCE:
			printf(" -> %s", TargetToString(node->target, arena).CString());
			break;
		default:
			break;
	}

	arena->head = mark;

	for (Node* child = node->child; child; child = child->next)
	{
		printf("\n%*s\x1b[90m[%s]\x1b[0m = ", (indent + 1) * 2, "", UsageToString(child->usage).CString());
		DumpNode(child, arena, indent + 1);
	}
}

void SerializeNode(StringBuilder& builder, Node* node)
{
	builder.AppendFormat("([%s]%s", UsageToString(node->usage).CString(), NodeTypeToString(node->node_type).CString());
	if (node->name != Atom::NONE)
	{
		builder.Append(" ");
		builder.Append(GetAtomString(node->name, builder.arena));
	}

	switch (node->node_type)
	{
		case NodeType::NUMBER:
			builder.AppendFormat(" %s", node->text);
			break;
		case NodeType::BOOL:
			builder.Append(node->value ? " true" : " false");
			break;
		case NodeType::ENUM_VALUE:
			builder.AppendFormat(" %lld", (long long)node->enum_value);
			break;
		case NodeType::UNARY:
		case NodeType::POSTFIX:
		case NodeType::BINARY:
			builder.Append(" ");
			builder.Append(TokenToString(node->op));
			break;
		case NodeType::REFERENCE:
			builder.Append(" -> ");
			builder.Append(TargetToString(node->target, builder.arena));
			break;
		default:
			break;
	}

	for (Node* child = node->child; child; child = child->next)
	{
		builder.Append(" ");
		SerializeNode(builder, child);
	}
	builder.Append(")");
}

}
