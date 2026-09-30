#include <EVA/Script/Script.hpp>
#include <stdarg.h>
#include <string.h>

namespace EVA::Script
{

// Returns false / nullptr from the calling function if expr is falsy.
#define TRY(expr)      \
	do                 \
	{                  \
		if (!(expr))   \
			return {}; \
	} while (0)

static bool Error(Parser& parser, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(parser.error_buffer, sizeof(parser.error_buffer), format, args);
	va_end(args);
	return false;
}

static bool UnexpectedToken(Parser& parser)
{
	Token& token = parser.token;
	if (token.token_type == TokenType::END_OF_FILE)
		return Error(parser, "unexpected end of file");
	return Error(parser, "unexpected token '%.*s'", (int)(token.end - token.start), token.start);
}

// Eats the current token if it's token_type, errors otherwise.
static bool ExpectToken(Parser& parser, TokenType token_type)
{
	TRY(LexToken(parser));
	Token& token = parser.token;
	if (token.token_type != token_type)
	{
		if (token.token_type == TokenType::END_OF_FILE)
			return Error(parser, "unexpected end of file while looking for token %d", (int)token_type);
		return Error(parser, "unexpected token '%.*s' while looking for token %d", (int)(token.end - token.start), token.start, (int)token_type);
	}
	EatToken(parser);
	return true;
}

// Eats the current token if it's an identifier, errors otherwise.
static bool ExpectIdentifier(Parser& parser, Atom* out_name)
{
	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::IDENTIFIER)
		return UnexpectedToken(parser);
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

static const uint32 MAX_NESTING_DEPTH = 256;
static const uint32 PREFIX_PRECEDENCE = 13;
static const uint32 DECLARATION_PRECEDENCE = 2;
static const uint32 ASSIGNMENT_PRECEDENCE = 1;

// Bounds recursion so untrusted input can't overflow the stack. Pair with DEFER(parser.depth--).
static bool EnterNesting(Parser& parser)
{
	if (parser.depth >= MAX_NESTING_DEPTH)
		return Error(parser, "nested too deeply");
	parser.depth++;
	return true;
}

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
	return precedence == ASSIGNMENT_PRECEDENCE || precedence == PREFIX_PRECEDENCE;
}

static ANode* NewNode(Parser& parser, NodeType type)
{
	ANode* node = parser.arena->New<ANode>();
	node->type = type;
	return node;
}

static AOperator* NewOperator(Parser& parser, NodeType type, TokenType op)
{
	AOperator* node = parser.arena->New<AOperator>();
	node->type = type;
	node->op = op;
	return node;
}

static ANode* PopOperand(Parser& parser)
{
	ANode* node = parser.operands.back();
	parser.operands.pop_back();
	return node;
}

// Pops the top pending operator and its operands, pushing the resulting node as an operand.
static void ApplyOperator(Parser& parser)
{
	PendingOp op = parser.operators.back();
	parser.operators.pop_back();

	ANode* node = nullptr;
	switch (op.kind)
	{
	case OpKind::PREFIX:
	{
		node = NewOperator(parser, NodeType::UNARY, op.op);
		node->child = PopOperand(parser);
		break;
	}
	case OpKind::ARRAY:
	{
		node = NewNode(parser, NodeType::ARRAY);
		node->child = op.payload;
		op.payload->next = PopOperand(parser);
		break;
	}
	case OpKind::INFIX:
	{
		ANode* right = PopOperand(parser);
		ANode* left = PopOperand(parser);
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

static ANode* ParseExpression(Parser& parser);
static ANode* ParseBlock(Parser& parser);

// Parses comma-separated expressions up to and including the closing token.
// out_arguments receives the first argument, the rest are chained via next. Empty lists and a trailing comma are allowed.
static bool ParseArguments(Parser& parser, TokenType closing, ANode** out_arguments)
{
	ANode** tail = out_arguments;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == closing)
		{
			EatToken(parser);
			return true;
		}

		ANode* argument = ParseExpression(parser);
		TRY(argument);
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

// Parses any number of '@' expression. *out_attribute_list is an ATTRIBUTE_LIST, or nullptr if there's no '@'.
static bool ParseAttributeList(Parser& parser, ANode** out_attribute_list)
{
	*out_attribute_list = nullptr;
	ANode** tail = nullptr;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type != TokenType::AT)
			return true;
		EatToken(parser);

		if (!*out_attribute_list)
		{
			*out_attribute_list = NewNode(parser, NodeType::ATTRIBUTE_LIST);
			tail = &(*out_attribute_list)->child;
		}

		ANode* attribute = ParseExpression(parser);
		TRY(attribute);
		*tail = attribute;
		tail = &attribute->next;
	}
}

static AIf* ParseIf(Parser& parser);

// The then / else part of an if: a block or an expression.
static ANode* ParseBranch(Parser& parser)
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
static AIf* ParseIf(Parser& parser)
{
	TRY(EnterNesting(parser));
	DEFER(parser.depth--);

	EatToken(parser);
	AIf* node = parser.arena->New<AIf>();
	node->type = NodeType::IF;

	node->condition = ParseExpression(parser);
	TRY(node->condition);
	node->then = ParseBranch(parser);
	TRY(node->then);

	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::KW_ELSE)
	{
		EatToken(parser);
		node->otherwise = ParseBranch(parser);
		TRY(node->otherwise);
	}
	return node;
}

// Whether the node's source ends with a '}' of a block, so as a statement it doesn't need a ';'.
static bool EndsWithBlock(ANode* node)
{
	if (node->type == NodeType::BLOCK)
		return true;
	if (node->type == NodeType::IF)
	{
		AIf* if_node = (AIf*)node;
		return EndsWithBlock(if_node->otherwise ? if_node->otherwise : if_node->then);
	}
	return false;
}

// Shunting yard over prefix and infix operators. Anything bracketed is parsed recursively into a single operand,
// and postfix operators wrap the top operand directly since they bind tighter than everything else.
// Stops at the first token that can't continue the expression, leaving it for the caller.
// A leading attribute list is attached to the resulting node.
static ANode* ParseExpression(Parser& parser)
{
	TRY(EnterNesting(parser));
	DEFER(parser.depth--);

	ANode* attribute_list = nullptr;
	TRY(ParseAttributeList(parser, &attribute_list));

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
				ANode* node = NewNode(parser, NodeType::IDENTIFIER);
				node->name = parser.token.atom;
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::NUMBER:
			{
				ANumber* node = parser.arena->New<ANumber>();
				node->type = NodeType::NUMBER;
				node->text = CopyText(parser, parser.token);
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::KW_TRUE:
			case TokenType::KW_FALSE:
			{
				ABool* node = parser.arena->New<ABool>();
				node->type = NodeType::BOOL;
				node->value = token_type == TokenType::KW_TRUE;
				EatToken(parser);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::KW_IF:
			{
				ANode* node = ParseIf(parser);
				TRY(node);
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_PAREN:
			{
				EatToken(parser);
				ANode* inner = ParseExpression(parser);
				TRY(inner);
				TRY(ExpectToken(parser, TokenType::RIGHT_PAREN));
				parser.operands.push_back(inner);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_BRACE:
			{
				EatToken(parser);
				ANode* node = NewNode(parser, NodeType::INIT_LIST);
				TRY(ParseArguments(parser, TokenType::RIGHT_BRACE, &node->child));
				parser.operands.push_back(node);
				expect_operand = false;
				break;
			}
			case TokenType::LEFT_BRACKET:
			{
				// [size]element. '[' can't otherwise start an operand, so this is never ambiguous with indexing.
				EatToken(parser);
				ANode* size = ParseExpression(parser);
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
			ANode* callee = PopOperand(parser);
			ANode* call = NewNode(parser, NodeType::CALL);
			call->child = callee;
			TRY(ParseArguments(parser, TokenType::RIGHT_PAREN, &callee->next));
			parser.operands.push_back(call);
			continue;
		}

		// foo[bar]:
		if (token_type == TokenType::LEFT_BRACKET)
		{
			EatToken(parser);
			ANode* object = PopOperand(parser);
			ANode* index = ParseExpression(parser);
			TRY(index);
			TRY(ExpectToken(parser, TokenType::RIGHT_BRACKET));
			ANode* node = NewNode(parser, NodeType::INDEX);
			node->child = object;
			object->next = index;
			parser.operands.push_back(node);
			continue;
		}

		// foo.bar:
		if (token_type == TokenType::DOT)
		{
			EatToken(parser);
			ANode* member = NewNode(parser, NodeType::MEMBER);
			TRY(ExpectIdentifier(parser, &member->name));
			member->child = PopOperand(parser);
			parser.operands.push_back(member);
			continue;
		}

		if (token_type == TokenType::INCREMENT || token_type == TokenType::DECREMENT)
		{
			EatToken(parser);
			ANode* node = NewOperator(parser, NodeType::POSTFIX, token_type);
			node->child = PopOperand(parser);
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

	ANode* result = PopOperand(parser);
	assert(parser.operands.size() == operand_base);

	if (attribute_list)
	{
		// @a (@b x): the outer attributes go first.
		if (result->attribute_list)
		{
			ANode* last = attribute_list->child;
			while (last->next)
				last = last->next;
			last->next = result->attribute_list->child;
		}
		result->attribute_list = attribute_list;
	}
	return result;
}

static bool ParseDeclaration(Parser& parser, ANode** out_declaration);

// return [value];
static ANode* ParseReturn(Parser& parser)
{
	EatToken(parser);
	ANode* node = NewNode(parser, NodeType::RETURN);

	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::SEMICOLON)
	{
		node->child = ParseExpression(parser);
		TRY(node->child);
	}
	TRY(ExpectToken(parser, TokenType::SEMICOLON));
	return node;
}

static ANode* ParseStatement(Parser& parser)
{
	TRY(EnterNesting(parser));
	DEFER(parser.depth--);

	TRY(LexToken(parser));
	switch (parser.token.token_type)
	{
	case TokenType::KW_RETURN: return ParseReturn(parser);
	case TokenType::LEFT_BRACE: return ParseBlock(parser);
	case TokenType::KW_IF:
	{
		// Parsed directly rather than via ParseExpression, so if a {} else {} doesn't continue into the next statement.
		ANode* node = ParseIf(parser);
		TRY(node);
		if (!EndsWithBlock(node))
			TRY(ExpectToken(parser, TokenType::SEMICOLON));
		return node;
	}
	default: break;
	}

	ANode* declaration = nullptr;
	TRY(ParseDeclaration(parser, &declaration));
	if (declaration)
		return declaration;

	ANode* expression = ParseExpression(parser);
	TRY(expression);
	TRY(ExpectToken(parser, TokenType::SEMICOLON));
	return expression;
}

// { statements }. out_statements receives the first statement, the rest are chained via next.
static bool ParseStatementList(Parser& parser, ANode** out_statements)
{
	TRY(ExpectToken(parser, TokenType::LEFT_BRACE));
	ANode** tail = out_statements;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::RIGHT_BRACE)
		{
			EatToken(parser);
			return true;
		}

		ANode* statement = ParseStatement(parser);
		TRY(statement);
		*tail = statement;
		tail = &statement->next;
	}
}

static ANode* ParseBlock(Parser& parser)
{
	ANode* node = NewNode(parser, NodeType::BLOCK);
	TRY(ParseStatementList(parser, &node->child));
	return node;
}

// [attributes] name: type
static ANode* ParseParameter(Parser& parser)
{
	ANode* node = NewNode(parser, NodeType::PARAMETER);
	TRY(ParseAttributeList(parser, &node->attribute_list));
	TRY(ExpectIdentifier(parser, &node->name));
	TRY(ExpectToken(parser, TokenType::COLON));
	node->child = ParseExpression(parser);
	TRY(node->child);
	return node;
}

// function name(parameters) [: return_type] { body }
static ANode* ParseFunction(Parser& parser)
{
	EatToken(parser);
	AFunction* node = parser.arena->New<AFunction>();
	node->type = NodeType::FUNCTION;
	TRY(ExpectIdentifier(parser, &node->name));

	TRY(ExpectToken(parser, TokenType::LEFT_PAREN));
	ANode** tail = &node->params;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::RIGHT_PAREN)
		{
			EatToken(parser);
			break;
		}

		ANode* param = ParseParameter(parser);
		TRY(param);
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
		node->return_type = ParseExpression(parser);
		TRY(node->return_type);
	}

	node->body = ParseBlock(parser);
	TRY(node->body);
	return node;
}

// Parses a const, struct or function declaration. Returns true with *out_declaration = nullptr, eating nothing,
// if the current token doesn't start one.
static bool ParseDeclaration(Parser& parser, ANode** out_declaration)
{
	*out_declaration = nullptr;
	TRY(LexToken(parser));
	switch (parser.token.token_type)
	{
	case TokenType::KW_CONST:
	{
		// const expression, where the expression is typically name: type = value. Self-terminating, no ';'.
		EatToken(parser);
		ANode* node = NewNode(parser, NodeType::CONST);
		node->child = ParseExpression(parser);
		TRY(node->child);
		*out_declaration = node;
		return true;
	}
	case TokenType::KW_STRUCT:
	{
		// struct name { members }
		EatToken(parser);
		ANode* node = NewNode(parser, NodeType::STRUCT);
		TRY(ExpectIdentifier(parser, &node->name));
		TRY(ParseStatementList(parser, &node->child));
		*out_declaration = node;
		return true;
	}
	case TokenType::KW_FUNCTION:
	{
		*out_declaration = ParseFunction(parser);
		return *out_declaration != nullptr;
	}
	default: return true;
	}
}

bool Parse(Parser& parser, ANode** out_declarations)
{
	ANode** tail = out_declarations;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::END_OF_FILE)
			return true;

		ANode* declaration = nullptr;
		TRY(ParseDeclaration(parser, &declaration));
		if (!declaration)
			return UnexpectedToken(parser);
		*tail = declaration;
		tail = &declaration->next;
	}
}

}
