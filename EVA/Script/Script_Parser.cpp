#include <EVA/Script/Script.hpp>
#include <stdarg.h>
#include <string.h>

namespace EVA::Script
{

// What ShapeDeclaration requires to be present, combined with |.
enum DeclarationRequire : uint32
{
	REQUIRE_TYPE = 1 << 0,
	REQUIRE_VALUE = 1 << 1,
};

// Returns false / nullptr from the calling function if expr is falsy.
#define TRY(expr)      \
	do                 \
	{                  \
		if (!(expr))   \
			return {}; \
	} while (0)

ScriptError* EmitError(Parser& parser, const char* format, ...)
{
	ScriptError* error = parser.error_arena->New<ScriptError>();
	va_list args;
	va_start(args, format);
	error->message = avprintf(parser.error_arena, format, args);
	va_end(args);
	parser.errors.push_back(error);
	return error;
}

static ScriptError* UnexpectedToken(Parser& parser)
{
	Token& token = parser.token;
	if (token.token_type == TokenType::END_OF_FILE)
		return EmitError(parser, "unexpected end of file");
	return EmitError(parser, "unexpected token '%.*s'", (int)(token.end - token.start), token.start);
}

// Eats the current token if it's token_type, errors otherwise.
static bool ExpectToken(Parser& parser, TokenType token_type)
{
	TRY(LexToken(parser));
	Token& token = parser.token;
	if (token.token_type != token_type)
	{
		if (token.token_type == TokenType::END_OF_FILE)
			EmitError(parser, "unexpected end of file, expected '%s'", TokenToString(token_type).CString());
		else
			EmitError(parser, "unexpected token '%.*s', expected '%s'", (int)(token.end - token.start), token.start,
				TokenToString(token_type).CString());
		return false;
	}
	EatToken(parser);
	return true;
}

// Eats the current token if it's an identifier, errors otherwise.
static bool ExpectIdentifier(Parser& parser, Atom* out_name)
{
	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::IDENTIFIER)
	{
		UnexpectedToken(parser);
		return false;
	}
	*out_name = parser.token.atom;
	EatToken(parser);
	return true;
}

// Copies the token's text into the arena, NUL-terminated.
static char* CopyText(Parser& parser, Token token)
{
	size_t length = token.end - token.start;
	char* text = (char*)parser.arena->Allocate(length + 1, 1);
	memcpy(text, token.start, length);
	text[length] = '\0';
	return text;
}

uint32 RECURSION_LIMIT = 256;

static const uint32 PREFIX_PRECEDENCE = 13;
static const uint32 DECLARATION_PRECEDENCE = 2;
static const uint32 ASSIGNMENT_PRECEDENCE = 1;

// Higher binds tighter. 0 if the token isn't a binary operator.
static uint32 BinaryPrecedence(TokenType op)
{
	switch (op)
	{
	case TokenType::ASTERISK:
	case TokenType::SLASH:
	case TokenType::PERCENT: return 12;
	case TokenType::PLUS:
	case TokenType::MINUS: return 11;
	case TokenType::SHIFT_LEFT:
	case TokenType::SHIFT_RIGHT: return 10;
	case TokenType::LESS:
	case TokenType::GREATER:
	case TokenType::LESS_EQUAL:
	case TokenType::GREATER_EQUAL: return 9;
	case TokenType::EQUAL:
	case TokenType::NOT_EQUAL: return 8;
	case TokenType::AMPERSAND: return 7;
	case TokenType::CARET: return 6;
	case TokenType::PIPE: return 5;
	case TokenType::LOGICAL_AND: return 4;
	case TokenType::LOGICAL_OR: return 3;
	case TokenType::COLON: return DECLARATION_PRECEDENCE;
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
	case TokenType::SHIFT_RIGHT_ASSIGN: return ASSIGNMENT_PRECEDENCE;
	default: return 0;
	}
}

static uint32 Precedence(const PendingOp& op)
{
	switch (op.kind)
	{
	case OpKind::PREFIX:
	case OpKind::ARRAY: return PREFIX_PRECEDENCE;
	case OpKind::INFIX: return BinaryPrecedence(op.op);
	}
	return 0;
}

static bool IsRightAssociative(uint32 precedence)
{
	// ':' is never valid chained, but groups as a : (b : c) for consistency.
	return precedence == ASSIGNMENT_PRECEDENCE || precedence == DECLARATION_PRECEDENCE || precedence == PREFIX_PRECEDENCE;
}

static Node* NewNode(Parser& parser, NodeType type)
{
	Node* node = parser.arena->New<Node>();
	node->type = type;
	return node;
}

static Node* NewOperator(Parser& parser, NodeType type, TokenType op)
{
	Node* node = NewNode(parser, type);
	node->op = op;
	return node;
}

static Node* PopOperand(Parser& parser)
{
	Node* node = parser.operands.back();
	parser.operands.pop_back();
	return node;
}

// Pops the top pending operator and its operands, pushing the resulting node as an operand.
static void ApplyOperator(Parser& parser)
{
	PendingOp op = parser.operators.back();
	parser.operators.pop_back();

	Node* node = nullptr;
	switch (op.kind)
	{
	case OpKind::PREFIX:
	{
		node = NewOperator(parser, NodeType::UNARY, op.op);
		node->child = PopOperand(parser);
		node->child->usage = Usage::OPERAND;
		break;
	}
	case OpKind::ARRAY:
	{
		node = NewNode(parser, NodeType::ARRAY_TYPE);
		Node* element = PopOperand(parser);
		op.payload->usage = Usage::SIZE;
		element->usage = Usage::ELEMENT;
		node->child = op.payload;
		op.payload->next = element;
		break;
	}
	case OpKind::INFIX:
	{
		Node* right = PopOperand(parser);
		Node* left = PopOperand(parser);
		left->usage = Usage::LEFT;
		right->usage = Usage::RIGHT;
		node = NewOperator(parser, NodeType::BINARY, op.op);
		node->child = left;
		left->next = right;
		break;
	}
	}
	parser.operands.push_back(node);
}

// Applies pending operators that bind tighter than an incoming infix operator of the given precedence.
static void ApplyOperatorsAbove(Parser& parser, size_t operator_base, uint32 precedence)
{
	while (parser.operators.size() > operator_base)
	{
		uint32 top = Precedence(parser.operators.back());
		if (top < precedence || (top == precedence && IsRightAssociative(precedence)))
			break;
		ApplyOperator(parser);
	}
}

static Node* ParseBlock(Parser& parser);

// Parses comma-separated expressions up to and including the closing token, each with the given usage.
// out_arguments receives the first argument, the rest are chained via next. Empty lists and a trailing comma are allowed.
static bool ParseArguments(Parser& parser, TokenType closing, Usage usage, Node** out_arguments)
{
	Node** tail = out_arguments;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == closing)
		{
			EatToken(parser);
			return true;
		}

		Node* argument = ParseExpression(parser);
		TRY(argument);
		argument->usage = usage;
		*tail = argument;
		tail = &argument->next;

		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::COMMA)
		{
			EatToken(parser);
			continue;
		}
		return ExpectToken(parser, closing);
	}
}

// Parses any number of '@' expression. out_attributes receives the first attribute, the rest are chained via next.
static bool ParseAttributes(Parser& parser, Node** out_attributes)
{
	*out_attributes = nullptr;
	Node** tail = out_attributes;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type != TokenType::AT)
			return true;
		EatToken(parser);

		Node* attribute = ParseExpression(parser);
		TRY(attribute);
		attribute->usage = Usage::ATTRIBUTE;
		*tail = attribute;
		tail = &attribute->next;
	}
}

// Prepends attributes to the node's children.
static void AttachAttributes(Node* node, Node* attributes)
{
	if (!attributes)
		return;
	Node* last = attributes;
	while (last->next)
		last = last->next;
	last->next = node->child;
	node->child = attributes;
}

static Node* ParseIf(Parser& parser);

// The then / else part of an if: a block or an expression.
static Node* ParseBranch(Parser& parser)
{
	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::LEFT_BRACE)
		return ParseBlock(parser);
	// Parsed directly rather than via ParseExpression, so an else if chain ending in a block doesn't continue into
	// whatever comes after it.
	if (parser.token.token_type == TokenType::KW_IF)
		return ParseIf(parser);
	return ParseExpression(parser);
}

// Called with 'if' as the current token. if condition then [else otherwise], where then and otherwise are blocks or
// expressions. Neither the condition nor the branches need brackets, since expressions end on their own.
static Node* ParseIf(Parser& parser)
{
	CHECK_RECURSION(parser);

	EatToken(parser);
	Node* node = NewNode(parser, NodeType::IF);

	Node* condition = ParseExpression(parser);
	TRY(condition);
	condition->usage = Usage::CONDITION;
	node->child = condition;

	Node* then = ParseBranch(parser);
	TRY(then);
	then->usage = Usage::THEN;
	condition->next = then;

	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::KW_ELSE)
	{
		EatToken(parser);
		Node* otherwise = ParseBranch(parser);
		TRY(otherwise);
		otherwise->usage = Usage::ELSE;
		then->next = otherwise;
	}
	return node;
}

// Whether the node's source ends with a '}' of a block, so as a statement it doesn't need a ';'.
static bool EndsWithBlock(Node* node)
{
	if (node->type == NodeType::BLOCK)
		return true;
	if (node->type == NodeType::IF)
	{
		Node* otherwise = FindChild(node, Usage::ELSE);
		return EndsWithBlock(otherwise ? otherwise : FindChild(node, Usage::THEN));
	}
	return false;
}

// Shunting yard over prefix and infix operators. Anything bracketed is parsed recursively into a single operand,
// and postfix operators wrap the top operand directly since they bind tighter than everything else.
// Stops at the first token that can't continue the expression, leaving it for the caller.
// Leading attributes are attached to the resulting node.
Node* ParseExpression(Parser& parser)
{
	CHECK_RECURSION(parser);

	Node* attributes = nullptr;
	TRY(ParseAttributes(parser, &attributes));

	size_t operand_base = parser.operands.size();
	size_t operator_base = parser.operators.size();
	bool expect_operand = true;

	for (;;)
	{
		TRY(LexToken(parser));
		TokenType token_type = parser.token.token_type;

		if (expect_operand)
		{
			switch (token_type)
			{
			case TokenType::IDENTIFIER:
			{
				Node* node = NewNode(parser, NodeType::IDENTIFIER);
				node->name = parser.token.atom;
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::NUMBER:
			{
				Node* node = NewNode(parser, NodeType::NUMBER);
				node->text = CopyText(parser, parser.token);
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::KW_TRUE:
			case TokenType::KW_FALSE:
			{
				Node* node = NewNode(parser, NodeType::BOOL);
				node->value = token_type == TokenType::KW_TRUE;
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::KW_IF:
			{
				Node* node = ParseIf(parser);
				TRY(node);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_PAREN:
			{
				EatToken(parser);
				Node* inner = ParseExpression(parser);
				TRY(inner);
				TRY(ExpectToken(parser, TokenType::RIGHT_PAREN));
				parser.operands.push_back(inner);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_BRACE:
			{
				EatToken(parser);
				Node* node = NewNode(parser, NodeType::INIT_LIST);
				TRY(ParseArguments(parser, TokenType::RIGHT_BRACE, Usage::ELEMENT, &node->child));
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_BRACKET:
			{
				// [size]element. '[' can't otherwise start an operand, so this is never ambiguous with indexing.
				EatToken(parser);
				Node* size = ParseExpression(parser);
				TRY(size);
				TRY(ExpectToken(parser, TokenType::RIGHT_BRACKET));
				parser.operators.push_back({ .op = token_type, .kind = OpKind::ARRAY, .payload = size });
				break;
			}
			case TokenType::MINUS:
			case TokenType::PLUS:
			case TokenType::EXCLAMATION:
			case TokenType::TILDE:
			case TokenType::INCREMENT:
			case TokenType::DECREMENT:
			{
				EatToken(parser);
				parser.operators.push_back({ .op = token_type, .kind = OpKind::PREFIX });
				break;
			}
			default:
			{
				UnexpectedToken(parser);
				return nullptr;
			}
			}
			continue;
		}

		// foo(arg1, arg2, arg3):
		if (token_type == TokenType::LEFT_PAREN)
		{
			EatToken(parser);
			Node* callee = PopOperand(parser);
			callee->usage = Usage::CALLEE;
			Node* call = NewNode(parser, NodeType::CALL);
			call->child = callee;
			TRY(ParseArguments(parser, TokenType::RIGHT_PAREN, Usage::ARGUMENT, &callee->next));
			parser.operands.push_back(call);
			continue;
		}

		// foo[bar]:
		if (token_type == TokenType::LEFT_BRACKET)
		{
			EatToken(parser);
			Node* object = PopOperand(parser);
			Node* index = ParseExpression(parser);
			TRY(index);
			TRY(ExpectToken(parser, TokenType::RIGHT_BRACKET));
			object->usage = Usage::OBJECT;
			index->usage = Usage::INDEX;
			Node* node = NewNode(parser, NodeType::INDEX);
			node->child = object;
			object->next = index;
			parser.operands.push_back(node);
			continue;
		}

		// foo.bar:
		if (token_type == TokenType::DOT)
		{
			EatToken(parser);
			Node* member = NewNode(parser, NodeType::MEMBER);
			TRY(ExpectIdentifier(parser, &member->name));
			member->child = PopOperand(parser);
			member->child->usage = Usage::OBJECT;
			parser.operands.push_back(member);
			continue;
		}

		// foo++, foo--:
		if (token_type == TokenType::INCREMENT || token_type == TokenType::DECREMENT)
		{
			EatToken(parser);
			Node* node = NewOperator(parser, NodeType::POSTFIX, token_type);
			node->child = PopOperand(parser);
			node->child->usage = Usage::OPERAND;
			parser.operands.push_back(node);
			continue;
		}

		uint32 precedence = BinaryPrecedence(token_type);
		if (!precedence)
			break;

		EatToken(parser);
		ApplyOperatorsAbove(parser, operator_base, precedence);
		parser.operators.push_back({ .op = token_type, .kind = OpKind::INFIX });
		expect_operand = true;
	}

	while (parser.operators.size() > operator_base)
		ApplyOperator(parser);

	Node* result = PopOperand(parser);
	assert(parser.operands.size() == operand_base);

	// @a (@b x): prepending puts the outer attributes first.
	AttachAttributes(result, attributes);
	return result;
}

static bool ParseDeclaration(Parser& parser, Node** out_declaration);

// return [value];
static Node* ParseReturn(Parser& parser)
{
	EatToken(parser);
	Node* node = NewNode(parser, NodeType::RETURN);

	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::SEMICOLON)
	{
		node->child = ParseExpression(parser);
		TRY(node->child);
		node->child->usage = Usage::VALUE;
	}
	TRY(ExpectToken(parser, TokenType::SEMICOLON));
	return node;
}

Node* ParseStatement(Parser& parser)
{
	CHECK_RECURSION(parser);

	TRY(LexToken(parser));
	switch (parser.token.token_type)
	{
	case TokenType::KW_RETURN: return ParseReturn(parser);
	case TokenType::LEFT_BRACE: return ParseBlock(parser);
	case TokenType::KW_IF:
	{
		// Parsed directly rather than via ParseExpression, so if a {} else {} doesn't continue into the next statement.
		Node* node = ParseIf(parser);
		TRY(node);
		if (!EndsWithBlock(node))
			TRY(ExpectToken(parser, TokenType::SEMICOLON));
		return node;
	}
	default: break;
	}

	Node* declaration = nullptr;
	TRY(ParseDeclaration(parser, &declaration));
	if (declaration)
		return declaration;

	Node* expression = ParseExpression(parser);
	TRY(expression);
	TRY(ExpectToken(parser, TokenType::SEMICOLON));
	return expression;
}

// { statements }, each with the given usage. out_statements receives the first statement, the rest are chained via next.
static bool ParseStatementList(Parser& parser, Usage usage, Node** out_statements)
{
	TRY(ExpectToken(parser, TokenType::LEFT_BRACE));
	Node** tail = out_statements;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::RIGHT_BRACE)
		{
			EatToken(parser);
			return true;
		}

		Node* statement = ParseStatement(parser);
		TRY(statement);
		statement->usage = usage;
		*tail = statement;
		tail = &statement->next;
	}
}

static Node* ParseBlock(Parser& parser)
{
	Node* node = NewNode(parser, NodeType::BLOCK);
	TRY(ParseStatementList(parser, Usage::STATEMENT, &node->child));
	return node;
}

// Reshapes a parsed name [: type] [= value] expression in place into a declaration: the root node gets the name and
// TYPE / VALUE children, the identifier and operator nodes are dropped. The caller sets the node type and attributes.
static bool ShapeDeclaration(Parser& parser, Node* node, DeclarationRequire required)
{
	Node* head = node;
	Node* type = nullptr;
	Node* value = nullptr;

	if (head->type == NodeType::BINARY && head->op == TokenType::EQUALS)
	{
		value = FindChild(head, Usage::RIGHT);
		head = FindChild(head, Usage::LEFT);
	}
	if (head->type == NodeType::BINARY && head->op == TokenType::COLON)
	{
		type = FindChild(head, Usage::RIGHT);
		head = FindChild(head, Usage::LEFT);
	}
	if (head->type != NodeType::IDENTIFIER)
	{
		EmitError(parser, "expected name [: type] [= value]");
		return false;
	}
	if ((required & REQUIRE_TYPE) && !type)
	{
		EmitError(parser, "'%s' needs a type", GetAtomString(head->name, parser.arena).CString());
		return false;
	}
	if ((required & REQUIRE_VALUE) && !value)
	{
		EmitError(parser, "'%s' needs a value", GetAtomString(head->name, parser.arena).CString());
		return false;
	}

	node->name = head->name;
	node->text = nullptr;
	Node** tail = &node->child;
	if (type)
	{
		type->usage = Usage::TYPE;
		*tail = type;
		tail = &type->next;
	}
	if (value)
	{
		value->usage = Usage::VALUE;
		*tail = value;
		tail = &value->next;
	}
	*tail = nullptr;
	return true;
}

// [attributes] name: type [= value]
static Node* ParseTypedDeclaration(Parser& parser, NodeType type)
{
	Node* attributes = nullptr;
	TRY(ParseAttributes(parser, &attributes));
	Node* node = ParseExpression(parser);
	TRY(node);
	TRY(ShapeDeclaration(parser, node, REQUIRE_TYPE));
	node->type = type;
	AttachAttributes(node, attributes);
	return node;
}

// function name(parameters) [: return_type] { body }
static Node* ParseFunction(Parser& parser)
{
	CHECK_RECURSION(parser); // functions can be declared in function bodies

	EatToken(parser);
	Node* node = NewNode(parser, NodeType::FUNCTION);
	TRY(ExpectIdentifier(parser, &node->name));

	TRY(ExpectToken(parser, TokenType::LEFT_PAREN));
	Node** tail = &node->child;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::RIGHT_PAREN)
		{
			EatToken(parser);
			break;
		}

		Node* param = ParseTypedDeclaration(parser, NodeType::PARAMETER);
		TRY(param);
		param->usage = Usage::PARAMETER;
		*tail = param;
		tail = &param->next;

		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::COMMA)
		{
			EatToken(parser);
			continue;
		}
		TRY(ExpectToken(parser, TokenType::RIGHT_PAREN));
		break;
	}

	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::COLON)
	{
		EatToken(parser);
		Node* return_type = ParseExpression(parser);
		TRY(return_type);
		return_type->usage = Usage::RETURN_TYPE;
		*tail = return_type;
		tail = &return_type->next;
	}

	Node* body = ParseBlock(parser);
	TRY(body);
	body->usage = Usage::BODY;
	*tail = body;
	return node;
}

// const name [: type] = value; The ';' is optional when the value ends with a block.
static Node* ParseConst(Parser& parser)
{
	EatToken(parser);
	Node* node = ParseExpression(parser);
	TRY(node);
	TRY(ShapeDeclaration(parser, node, REQUIRE_VALUE));
	node->type = NodeType::CONST;

	if (EndsWithBlock(FindChild(node, Usage::VALUE)))
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::SEMICOLON)
			EatToken(parser);
	}
	else
	{
		TRY(ExpectToken(parser, TokenType::SEMICOLON));
	}
	return node;
}

// struct name { [attributes] name: type [= value]; ... }
static Node* ParseStruct(Parser& parser)
{
	CHECK_RECURSION(parser); // structs don't nest yet, but may

	EatToken(parser);
	Node* node = NewNode(parser, NodeType::STRUCT);
	TRY(ExpectIdentifier(parser, &node->name));

	TRY(ExpectToken(parser, TokenType::LEFT_BRACE));
	Node** tail = &node->child;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::RIGHT_BRACE)
		{
			EatToken(parser);
			return node;
		}

		Node* field = ParseTypedDeclaration(parser, NodeType::FIELD);
		TRY(field);
		TRY(ExpectToken(parser, TokenType::SEMICOLON));
		field->usage = Usage::MEMBER;
		*tail = field;
		tail = &field->next;
	}
}

// Parses a const, struct or function declaration with its leading attributes. Returns true with
// *out_declaration = nullptr, eating nothing, if the current token doesn't start one.
static bool ParseDeclaration(Parser& parser, Node** out_declaration)
{
	*out_declaration = nullptr;
	Node* attributes = nullptr;
	TRY(ParseAttributes(parser, &attributes));

	Node* node = nullptr;
	TRY(LexToken(parser));
	switch (parser.token.token_type)
	{
	case TokenType::KW_CONST: node = ParseConst(parser); break;
	case TokenType::KW_STRUCT: node = ParseStruct(parser); break;
	case TokenType::KW_FUNCTION: node = ParseFunction(parser); break;
	default:
		// The attributes are already eaten, so the caller can't parse them as something else.
		if (attributes)
		{
			UnexpectedToken(parser);
			return false;
		}
		return true;
	}
	TRY(node);
	AttachAttributes(node, attributes);
	*out_declaration = node;
	return true;
}

bool Parse(Parser& parser, Node** out_module)
{
	Node* module = NewNode(parser, NodeType::MODULE);
	module->usage = Usage::ROOT;
	*out_module = module;

	Node** tail = &module->child;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::END_OF_FILE)
			return true;

		Node* declaration = nullptr;
		TRY(ParseDeclaration(parser, &declaration));
		if (!declaration)
		{
			UnexpectedToken(parser);
			return false;
		}
		declaration->usage = Usage::DECLARATION;
		*tail = declaration;
		tail = &declaration->next;
	}
}

}
