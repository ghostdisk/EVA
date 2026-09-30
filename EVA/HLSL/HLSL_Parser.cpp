#include <EVA/HLSL/HLSL.hpp>
#include <stdarg.h>
#include <string.h>
#include <unordered_set>

namespace EVA::HLSL
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

// Copies the token's text into the arena, NUL-terminated.
static char* CopyText(Parser& parser, Token token)
{
	size_t length = token.end - token.start;
	char* text = (char*)parser.arena->Allocate(length + 1, 1);
	memcpy(text, token.start, length);
	text[length] = '\0';
	return text;
}

// If the current token is '>>', shrinks it to its first '>'. Eating it then leaves the second '>' to be lexed next.
static void SplitShiftRight(Parser& parser)
{
	if (parser.token.token_type != TokenType::SHIFT_RIGHT)
		return;
	parser.token.token_type = TokenType::GREATER;
	parser.token.end = parser.token.start + 1;
}

static bool ParseAttributes(Parser& parser, Node** out_attributes)
{
	return Error(parser, "%s not implemented", __func__);
}

// Eats any number of modifier keywords. Which modifiers are valid where is checked by the caller.
static bool ParseModifiers(Parser& parser, uint32* out_modifiers)
{
	uint32 modifiers = 0;
	for (;;)
	{
		TRY(LexToken(parser));

		uint32 modifier = 0;
		switch (parser.token.token_type)
		{
		case TokenType::KW_CONST: modifier = MODIFIER_CONST; break;
		case TokenType::KW_STATIC: modifier = MODIFIER_STATIC; break;
		case TokenType::KW_EXTERN: modifier = MODIFIER_EXTERN; break;
		case TokenType::KW_UNIFORM: modifier = MODIFIER_UNIFORM; break;
		case TokenType::KW_EXPORT: modifier = MODIFIER_EXPORT; break;
		case TokenType::KW_INLINE: modifier = MODIFIER_INLINE; break;
		case TokenType::KW_GROUPSHARED: modifier = MODIFIER_GROUPSHARED; break;
		case TokenType::KW_GLOBALLYCOHERENT: modifier = MODIFIER_GLOBALLYCOHERENT; break;
		case TokenType::KW_PRECISE: modifier = MODIFIER_PRECISE; break;
		case TokenType::KW_ROW_MAJOR: modifier = MODIFIER_ROW_MAJOR; break;
		case TokenType::KW_COLUMN_MAJOR: modifier = MODIFIER_COLUMN_MAJOR; break;
		case TokenType::KW_SNORM: modifier = MODIFIER_SNORM; break;
		case TokenType::KW_UNORM: modifier = MODIFIER_UNORM; break;
		case TokenType::KW_IN: modifier = MODIFIER_IN; break;
		case TokenType::KW_OUT: modifier = MODIFIER_OUT; break;
		case TokenType::KW_INOUT: modifier = MODIFIER_INOUT; break;
		case TokenType::KW_NOINTERPOLATION: modifier = MODIFIER_NOINTERPOLATION; break;
		case TokenType::KW_NOPERSPECTIVE: modifier = MODIFIER_NOPERSPECTIVE; break;
		case TokenType::KW_CENTROID: modifier = MODIFIER_CENTROID; break;
		default: break;
		}
		if (!modifier)
			break;

		if (modifiers & modifier)
			return Error(parser, "duplicate modifier '%.*s'", (int)(parser.token.end - parser.token.start), parser.token.start);
		modifiers |= modifier;
		EatToken(parser);
	}

	*out_modifiers = modifiers;
	return true;
}

// Hardcoded until structs and typedefs register themselves in a type table.
static const char* template_names[] = {
	"vector",
	"matrix",
	"Buffer",
	"RWBuffer",
	"StructuredBuffer",
	"RWStructuredBuffer",
	"AppendStructuredBuffer",
	"ConsumeStructuredBuffer",
	"Texture1D",
	"Texture1DArray",
	"Texture2D",
	"Texture2DArray",
	"Texture2DMS",
	"Texture2DMSArray",
	"Texture3D",
	"TextureCube",
	"TextureCubeArray",
	"RWTexture1D",
	"RWTexture1DArray",
	"RWTexture2D",
	"RWTexture2DArray",
	"RWTexture3D",
};

static const char* scalar_type_names[] = {
	"bool",
	"int",
	"uint",
	"dword",
	"half",
	"float",
	"double",
	"min16float",
	"min10float",
	"min16int",
	"min12int",
	"min16uint",
	"int16_t",
	"int32_t",
	"int64_t",
	"uint16_t",
	"uint32_t",
	"uint64_t",
	"float16_t",
	"float32_t",
	"float64_t",
};

static const char* other_type_names[] = {
	"void",
	"SamplerState",
	"SamplerComparisonState",
	"ByteAddressBuffer",
	"RWByteAddressBuffer",
};

struct BuiltinTypeNames
{
	std::unordered_set<Atom> templates;
	std::unordered_set<Atom> types; // everything that names a type, including the templates
};

static BuiltinTypeNames CreateBuiltinTypeNames()
{
	BuiltinTypeNames names;
	for (const char* name : template_names)
	{
		names.templates.insert(GetAtom(name));
		names.types.insert(GetAtom(name));
	}
	for (const char* name : other_type_names)
		names.types.insert(GetAtom(name));

	// Every scalar, optionally followed by a vector size N or a matrix size NxM, with N and M in 1..4.
	char buffer[64];
	for (const char* scalar : scalar_type_names)
	{
		names.types.insert(GetAtom(scalar));
		for (int n = 1; n <= 4; ++n)
		{
			snprintf(buffer, sizeof(buffer), "%s%d", scalar, n);
			names.types.insert(GetAtom(buffer));
			for (int m = 1; m <= 4; ++m)
			{
				snprintf(buffer, sizeof(buffer), "%s%dx%d", scalar, n, m);
				names.types.insert(GetAtom(buffer));
			}
		}
	}
	return names;
}

static BuiltinTypeNames& GetBuiltinTypeNames()
{
	static BuiltinTypeNames names = CreateBuiltinTypeNames();
	return names;
}

static bool IsTemplateName(Node* node)
{
	return node->type == NodeType::IDENTIFIER && GetBuiltinTypeNames().templates.count(node->name);
}

static bool IsTypeName(Node* node)
{
	if (node->type == NodeType::TEMPLATE)
		return IsTemplateName(node->child);
	return node->type == NodeType::IDENTIFIER && GetBuiltinTypeNames().types.count(node->name);
}

static const uint32 MAX_EXPRESSION_DEPTH = 256;
static const uint32 PREFIX_PRECEDENCE = 13;
static const uint32 TERNARY_PRECEDENCE = 2;
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
	case OpKind::CAST: return PREFIX_PRECEDENCE;
	case OpKind::TERNARY: return TERNARY_PRECEDENCE;
	case OpKind::INFIX: return BinaryPrecedence(op.op);
	}
	return 0;
}

static bool IsRightAssociative(uint32 precedence)
{
	return precedence == ASSIGNMENT_PRECEDENCE || precedence == TERNARY_PRECEDENCE || precedence == PREFIX_PRECEDENCE;
}

static Node* NewNode(Parser& parser, NodeType type)
{
	Node* node = parser.arena->New<Node>();
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
		break;
	}
	case OpKind::CAST:
	{
		node = NewNode(parser, NodeType::CAST);
		node->child = op.payload;
		op.payload->next = PopOperand(parser);
		break;
	}
	case OpKind::INFIX:
	{
		Node* right = PopOperand(parser);
		Node* left = PopOperand(parser);
		node = NewOperator(parser, NodeType::BINARY, op.op);
		node->child = left;
		left->next = right;
		break;
	}
	case OpKind::TERNARY:
	{
		Node* otherwise = PopOperand(parser);
		Node* condition = PopOperand(parser);
		node = NewNode(parser, NodeType::TERNARY);
		node->child = condition;
		condition->next = op.payload;
		op.payload->next = otherwise;
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

static Node* ParseExpression(Parser& parser, bool in_template_arguments);

// Parses comma-separated expressions up to and including the closing token.
// out_arguments receives the first argument, the rest are chained via next. Empty lists are allowed.
static bool ParseArguments(Parser& parser, TokenType closing, Node** out_arguments)
{
	bool in_template_arguments = closing == TokenType::GREATER;
	Node** tail = out_arguments;

	TRY(LexToken(parser));
	if (in_template_arguments)
		SplitShiftRight(parser);
	if (parser.token.token_type == closing)
	{
		EatToken(parser);
		return true;
	}

	for (;;)
	{
		Node* argument = ParseExpression(parser, in_template_arguments);
		TRY(argument);
		*tail = argument;
		tail = &argument->next;

		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::COMMA)
		{
			EatToken(parser);
			continue;
		}
		if (in_template_arguments)
			SplitShiftRight(parser);
		return ExpectToken(parser, closing);
	}
}

// Shunting yard over prefix and infix operators. Anything bracketed is parsed recursively into a single operand,
// and postfix operators wrap the top operand directly since they bind tighter than everything else.
// Stops at the first token that can't continue the expression, leaving it for the caller.
// in_template_arguments: '>' ends the expression instead of being a comparison.
static Node* ParseExpression(Parser& parser, bool in_template_arguments)
{
	if (parser.depth >= MAX_EXPRESSION_DEPTH)
	{
		Error(parser, "expression nested too deeply");
		return nullptr;
	}
	parser.depth++;
	DEFER(parser.depth--);

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
			case TokenType::LEFT_PAREN:
			{
				EatToken(parser);
				Node* inner = ParseExpression(parser, false);
				TRY(inner);
				TRY(ExpectToken(parser, TokenType::RIGHT_PAREN));
				if (IsTypeName(inner))
				{
					// (type) is a cast, its operand comes next.
					parser.operators.push_back({ .op = TokenType::LEFT_PAREN, .kind = OpKind::CAST, .payload = inner });
				}
				else
				{
					parser.operands.push_back(inner);
					expect_operand = false;
				}
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
			Node* call = NewNode(parser, NodeType::CALL);
			call->child = callee;
			TRY(ParseArguments(parser, TokenType::RIGHT_PAREN, &callee->next));
			parser.operands.push_back(call);
			continue;
		}

		// foo<arg1, arg2, arg3>:
		if (token_type == TokenType::LESS && IsTemplateName(parser.operands.back()))
		{
			EatToken(parser);
			Node* name = PopOperand(parser);
			Node* instance = NewNode(parser, NodeType::TEMPLATE);
			instance->child = name;
			TRY(ParseArguments(parser, TokenType::GREATER, &name->next));
			if (!name->next)
			{
				Error(parser, "empty template argument list");
				return nullptr;
			}
			parser.operands.push_back(instance);
			continue;
		}

		// foo[bar]:
		if (token_type == TokenType::LEFT_BRACKET)
		{
			EatToken(parser);
			Node* object = PopOperand(parser);
			Node* index = ParseExpression(parser, false);
			TRY(index);
			TRY(ExpectToken(parser, TokenType::RIGHT_BRACKET));
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
			TRY(LexToken(parser));
			if (parser.token.token_type != TokenType::IDENTIFIER)
			{
				UnexpectedToken(parser);
				return nullptr;
			}
			Node* member = NewNode(parser, NodeType::MEMBER);
			member->name = parser.token.atom;
			EatToken(parser);
			member->child = PopOperand(parser);
			parser.operands.push_back(member);
			continue;
		}

		if (token_type == TokenType::INCREMENT || token_type == TokenType::DECREMENT)
		{
			EatToken(parser);
			Node* node = NewOperator(parser, NodeType::POSTFIX, token_type);
			node->child = PopOperand(parser);
			parser.operands.push_back(node);
			continue;
		}

		if (token_type == TokenType::QUESTION)
		{
			EatToken(parser);
			ApplyOperatorsAbove(parser, operator_base, TERNARY_PRECEDENCE);
			Node* then = ParseExpression(parser, false);
			TRY(then);
			TRY(ExpectToken(parser, TokenType::COLON));
			parser.operators.push_back({ .op = token_type, .kind = OpKind::TERNARY, .payload = then });
			expect_operand = true;
			continue;
		}

		// if we're in the middle of a template, stop on > (or >>, bleh)
		if (in_template_arguments && (token_type == TokenType::GREATER || token_type == TokenType::SHIFT_RIGHT))
			break;

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
	return result;
}

static Node* ParseType(Parser& parser)
{
	Node* type = ParseExpression(parser, false);
	TRY(type);
	if (!IsTypeName(type))
	{
		Error(parser, "expected a type");
		return nullptr;
	}
	return type;
}

static bool ParseStruct(Parser& parser)
{
	return Error(parser, "%s not implemented", __func__);
}

static bool ParseTypedef(Parser& parser)
{
	return Error(parser, "%s not implemented", __func__);
}

// Called with the name eaten and '(' as the current token.
static bool ParseFunction(Parser& parser, Node* attributes, uint32 modifiers, Node* return_type, Token name)
{
	return Error(parser, "%s not implemented", __func__);
}

// Called with the first name eaten. Parses the rest of the declarator list up to and including ';'.
static bool ParseVariables(Parser& parser, uint32 modifiers, Node* type, Token name)
{
	return Error(parser, "%s not implemented", __func__);
}

// Parses one top-level declaration, appending its node(s) via parser.tail.
static bool ParseTopLevel(Parser& parser)
{
	TRY(LexToken(parser));

	if (parser.token.token_type == TokenType::SEMICOLON)
	{
		EatToken(parser);
		return true;
	}

	if (parser.token.token_type == TokenType::KW_STRUCT)
		return ParseStruct(parser);

	if (parser.token.token_type == TokenType::KW_TYPEDEF)
		return ParseTypedef(parser);

	// modifiers* type name, followed by either '(' for a function or the rest of a variable declarator list.
	Node* attributes = nullptr;
	if (parser.token.token_type == TokenType::LEFT_BRACKET)
		TRY(ParseAttributes(parser, &attributes));

	uint32 modifiers = 0;
	TRY(ParseModifiers(parser, &modifiers));

	Node* type = ParseType(parser);
	TRY(type);

	TRY(LexToken(parser));
	if (parser.token.token_type != TokenType::IDENTIFIER)
		return UnexpectedToken(parser);
	Token name = parser.token;
	EatToken(parser);

	TRY(LexToken(parser));
	if (parser.token.token_type == TokenType::LEFT_PAREN)
		return ParseFunction(parser, attributes, modifiers, type, name);

	if (attributes)
		return Error(parser, "attributes are only allowed on functions");
	return ParseVariables(parser, modifiers, type, name);
}

bool Parse(Parser& parser, Node** out_declarations)
{
	parser.tail = out_declarations;
	for (;;)
	{
		TRY(LexToken(parser));
		if (parser.token.token_type == TokenType::END_OF_FILE)
			return true;
		TRY(ParseTopLevel(parser));
	}
}

}
