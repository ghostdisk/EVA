#include <EVA/Script/Script.hpp>
#include <stdio.h>

namespace EVA::Script
{

const char* NodeTypeName(NodeType type)
{
	switch (type)
	{
		case NodeType::NONE: return "NONE";
		case NodeType::CONST: return "CONST";
		case NodeType::STRUCT: return "STRUCT";
		case NodeType::FUNCTION: return "FUNCTION";
		case NodeType::PARAMETER: return "PARAMETER";
		case NodeType::FIELD: return "FIELD";
		case NodeType::BLOCK: return "BLOCK";
		case NodeType::RETURN: return "RETURN";
		case NodeType::NUMBER: return "NUMBER";
		case NodeType::BOOL: return "BOOL";
		case NodeType::IDENTIFIER: return "IDENTIFIER";
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

const char* UsageName(Usage usage)
{
	switch (usage)
	{
		case Usage::NONE: return "NONE";
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

static void PrintOperator(TokenType op)
{
	if ((uint8)op < 128)
	{
		printf("%c", (char)op);
		return;
	}

	const char* text = "?";
	switch (op)
	{
		case TokenType::ADD_ASSIGN: text = "+="; break;
		case TokenType::SUBTRACT_ASSIGN: text = "-="; break;
		case TokenType::MULTIPLY_ASSIGN: text = "*="; break;
		case TokenType::DIVIDE_ASSIGN: text = "/="; break;
		case TokenType::MODULO_ASSIGN: text = "%="; break;
		case TokenType::BIT_AND_ASSIGN: text = "&="; break;
		case TokenType::BIT_OR_ASSIGN: text = "|="; break;
		case TokenType::BIT_XOR_ASSIGN: text = "^="; break;
		case TokenType::SHIFT_LEFT_ASSIGN: text = "<<="; break;
		case TokenType::SHIFT_RIGHT_ASSIGN: text = ">>="; break;
		case TokenType::INCREMENT: text = "++"; break;
		case TokenType::DECREMENT: text = "--"; break;
		case TokenType::SHIFT_LEFT: text = "<<"; break;
		case TokenType::SHIFT_RIGHT: text = ">>"; break;
		case TokenType::EQUAL: text = "=="; break;
		case TokenType::NOT_EQUAL: text = "!="; break;
		case TokenType::LESS_EQUAL: text = "<="; break;
		case TokenType::GREATER_EQUAL: text = ">="; break;
		case TokenType::LOGICAL_AND: text = "&&"; break;
		case TokenType::LOGICAL_OR: text = "||"; break;
		default: break;
	}
	printf("%s", text);
}

// Prints node and its subtree, one node per line. The node's own line is started by the caller (indent and usage)
// and ended by the caller (newline), so root calls print no usage. The arena is only used for atom strings and is rewound afterwards.
void DumpNode(Node* node, Arena* arena, int indent)
{
	uint8* mark = arena->head;

	printf("\x1b[33m%s\x1b[0m", NodeTypeName(node->type));
	if (node->name != Atom::NONE)
		printf(" | %s", GetAtomString(node->name, arena).CString());

	switch (node->type)
	{
		case NodeType::NUMBER:
			printf(" | %s", node->text);
			break;
		case NodeType::BOOL:
			printf(" | %s", node->value ? "true" : "false");
			break;
		case NodeType::UNARY:
		case NodeType::POSTFIX:
		case NodeType::BINARY:
			printf(" | ");
			PrintOperator(node->op);
			break;
		default:
			break;
	}

	arena->head = mark;

	for (Node* child = node->child; child; child = child->next)
	{
		printf("\n%*s\x1b[90m[%s]\x1b[0m = ", (indent + 1) * 2, "", UsageName(child->usage));
		DumpNode(child, arena, indent + 1);
	}
}

}
