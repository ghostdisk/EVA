#include <EVA/Script/Fuzz/FuzzCommon.hpp>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>

// Fuzzes the compiler with generated programs. The input isn't source: it's read as a stream of decisions (which type,
// which expression, how many fields...) for a generator that knows the grammar and the type rules, so every input is a
// program that parses, and libFuzzer's coverage guidance works on the decisions. Raw source fuzzing rarely gets far
// enough to exercise the typer and the constant evaluator; this starts there.
//
// Valid mode generates only programs the compiler currently accepts, and computes what it expects for them
// independently: the value of every const, the type of every declaration, the layout of every struct, the inputs and
// outputs of every entry point. Any error or difference is a failure. When the language changes, this mode has to follow.
//
// Chaos mode, chosen by the first decision, mixes in type errors, unsupported constructs, deep nesting, bad attributes,
// cyclic structs, huge arrays, constants that grow exponentially and broken shader interfaces, and only checks what
// FuzzCommon checks for any input.
//
// Set EVA_FUZZ_PRINT=1 to print each generated program, e.g. to see what a crashing input generates.
// std::string and std::vector throughout, since this is tooling.

using namespace EVA;
using namespace EVA::Script;

namespace
{

// The fuzzer's input as decisions. Past the end every decision is 0, which always picks the simplest option, so any
// input makes a complete program and changing a byte changes one decision.
struct Decisions
{
	const uint8* data = nullptr;
	size_t size = 0;
	size_t position = 0;

	uint32 Byte() { return position < size ? data[position++] : 0; }

	// 0 to n - 1.
	uint32 Below(uint32 n)
	{
		if (n <= 1)
			return 0;
		uint32 bytes = n > 65536 ? 4 : n > 256 ? 2 : 1;
		uint32 value = 0;
		for (uint32 i = 0; i < bytes; ++i)
			value = value << 8 | Byte();
		return value % n;
	}

	uint32 Bits() { return Below(0xFFFFFFFF) ; }

	// True with the given chance, always false once the input runs out.
	bool Chance(uint32 percent) { return Below(100) + percent >= 100; }
};

enum class Kind : uint8
{
	INT,
	UINT,
	FLOAT,
	VECTOR, // float2 to float4
	ARRAY,
	STRUCT,
};

struct GenType;

struct GenField
{
	std::string name;
	GenType* type = nullptr;
	uint32 offset = 0;
	int32 semantic = -1; // the field's @semantic, a Semantic
	int32 location = -1; // or its @location
};

// The generator's own model of a type, laid out by the same rules as the compiler's.
struct GenType
{
	Kind kind = Kind::INT;
	std::string name; // as TypeToString prints it
	uint32 size = 4;
	uint32 alignment = 4;
	uint32 count = 1;          // VECTOR: components
	GenType* element = nullptr; // ARRAY
	uint32 length = 0;         // ARRAY
	uint32 stride = 0;         // ARRAY
	std::vector<GenField> fields; // STRUCT
	bool io = false;              // STRUCT: every leaf has a semantic or location, so it can be an entry point's input or output
	uint32 locations = 0;         // IO STRUCT: bit per location used by its leaves
	uint32 semantics = 0;         // IO STRUCT: bit per Semantic used by its leaves
};

const char* const SEMANTIC_NAMES[] = { "vertex_index", "position" }; // by Semantic
const uint32 VERTEX_INDEX_BIT = 1u << (uint32)Semantic::VERTEX_INDEX;
const uint32 POSITION_BIT = 1u << (uint32)Semantic::POSITION;
const uint32 LOCATION_COUNT = 32;

bool IsComposite(GenType* type)
{
	return type->kind == Kind::ARRAY || type->kind == Kind::STRUCT;
}

// The kind of each 4-byte component of a scalar or vector.
Kind ComponentKind(GenType* type)
{
	return type->kind == Kind::VECTOR ? Kind::FLOAT : type->kind;
}

uint64 RoundUp(uint64 value, uint64 alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

typedef std::vector<uint8> Bytes;

uint32 ReadComponent(const Bytes& bytes, uint32 index)
{
	uint32 bits;
	memcpy(&bits, bytes.data() + (size_t)index * 4, 4);
	return bits;
}

void WriteComponent(Bytes& bytes, uint32 index, uint32 bits)
{
	memcpy(bytes.data() + (size_t)index * 4, &bits, 4);
}

float ToFloat(uint32 bits)
{
	float value;
	memcpy(&value, &bits, 4);
	return value;
}

uint32 FromFloat(float value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	return bits;
}

// An expression and what the generator knows about it.
struct Expr
{
	std::string text;
	bool constant = false; // the compiler can evaluate it
	Bytes value;           // constant only: laid out like the compiler's Constant, padding zeroed
	bool literal = false;  // a bare NUMBER, which takes its type from the other operand of a binary operator
	bool hint_free = true; // types as intended without an expected type, so a const can leave its type out
};

// A name expressions can refer to.
struct Symbol
{
	std::string name;
	GenType* type = nullptr;
	bool constant = false;
	Bytes value; // constant only
};

// Where an expression goes.
struct Where
{
	bool constant_only = false; // a const's value, an array size, a location
	bool init_list = false;     // the expected type is known, so initializer lists work
};

struct Expected
{
	std::string name;
	GenType* type = nullptr;
	bool has_value = false;
	Bytes value;
};

const uint32 MAX_DEPTH = 5;
const uint32 MAX_TYPE_SIZE = 1024;   // in valid mode
const uint32 MAX_VALUE_SIZE = 65536; // values beyond aren't tracked
const uint32 MAX_INIT_LIST_SIZE = 4096; // larger initializer lists aren't written out, they'd take most of the time
const size_t MAX_SOURCE = (size_t)32 * 1024; // no more declarations past this

struct Generator
{
	Decisions decisions;
	bool chaos = false;

	std::vector<std::unique_ptr<GenType>> owned;
	GenType* int_type = nullptr;
	GenType* uint_type = nullptr;
	GenType* float_type = nullptr;
	GenType* vector_types[5] = {}; // by component count, 2 to 4
	std::vector<GenType*> array_types;
	std::vector<GenType*> struct_types;

	std::vector<Symbol> symbols; // in scope, innermost last
	std::vector<Expected> expected;
	std::string expected_interface; // as ShaderInterfaceToString prints it
	uint32 next_name = 0;
	size_t source_size = 0;

	uint32 Below(uint32 n) { return decisions.Below(n); }
	bool Chance(uint32 percent) { return decisions.Chance(percent); }

	std::string NewName(const char* prefix)
	{
		return prefix + std::to_string(next_name++);
	}

	GenType* NewType(Kind kind, std::string name, uint32 size, uint32 alignment)
	{
		owned.push_back(std::make_unique<GenType>());
		GenType* type = owned.back().get();
		type->kind = kind;
		type->name = std::move(name);
		type->size = size;
		type->alignment = alignment;
		return type;
	}

	void InitTypes()
	{
		int_type = NewType(Kind::INT, "int", 4, 4);
		uint_type = NewType(Kind::UINT, "uint", 4, 4);
		float_type = NewType(Kind::FLOAT, "float", 4, 4);
		for (uint32 count = 2; count <= 4; ++count)
		{
			vector_types[count] = NewType(Kind::VECTOR, "float" + std::to_string(count), 4 * count, 4);
			vector_types[count]->count = count;
		}
	}

	GenType* ArrayOf(GenType* element, uint32 length)
	{
		for (GenType* type : array_types)
		{
			if (type->element == element && type->length == length)
				return type;
		}
		uint64 stride = RoundUp(element->size, element->alignment);
		uint64 size = stride * length;
		if (size > UINT32_MAX)
			return nullptr;
		GenType* type = NewType(Kind::ARRAY, "[" + std::to_string(length) + "]" + element->name, (uint32)size, element->alignment);
		type->element = element;
		type->length = length;
		type->stride = (uint32)stride;
		array_types.push_back(type);
		return type;
	}

	// Types

	GenType* PickScalarOrVector()
	{
		switch (Below(6))
		{
		case 0: return float_type;
		case 1: return int_type;
		case 2: return uint_type;
		default: return vector_types[2 + Below(3)];
		}
	}

	// Any type, from the structs before max_struct. Composites only to some depth, and nothing larger than MAX_TYPE_SIZE
	// outside of chaos mode.
	GenType* PickType(uint32 max_struct, uint32 depth = 0)
	{
		uint32 choice = Below(depth < 3 ? 8 : 6);
		GenType* type = nullptr;
		if (choice < 6)
			type = PickScalarOrVector();
		else if (choice == 6)
		{
			GenType* element = PickType(max_struct, depth + 1);
			uint32 length = 1 + Below(4);
			if (chaos && Chance(5))
			{
				switch (Below(3))
				{
				case 0: length = 0xFFFFFFFF; break;
				case 1: length = 1 + Below(1u << 24); break;
				default: // just fits
					length = (uint32)(UINT32_MAX / RoundUp(element->size ? element->size : 1, element->alignment));
					length = length ? length : 1;
					break;
				}
			}
			type = ArrayOf(element, length);
		}
		else if (max_struct)
			type = struct_types[Below(max_struct)];
		if (!type || (!chaos && type->size > MAX_TYPE_SIZE))
			return float_type;
		return type;
	}

	// How to write length in an array type: a literal, a sum, or a const that has the value.
	std::string LengthText(uint32 length, bool allow_references)
	{
		if (chaos && Chance(5))
		{
			// Any name, so also consts that aren't integers, and things that aren't values.
			if (!symbols.empty() && Below(2))
				return symbols[Below((uint32)symbols.size())].name;
			const char* const bad[] = { "0", "-1", "1.5", "4294967296", "x", "float", "", "1, 2" };
			return bad[Below(8)];
		}
		switch (Below(4))
		{
		case 1:
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%X", length);
			return text;
		}
		case 2:
		{
			uint32 left = Below(length + 1);
			return "(" + std::to_string(left) + " + " + std::to_string(length - left) + ")";
		}
		case 3:
			if (allow_references)
			{
				for (size_t i = symbols.size(); i-- > 0;)
				{
					Symbol& symbol = symbols[i];
					if (symbol.constant && symbol.value.size() == 4 &&
						(symbol.type->kind == Kind::INT || symbol.type->kind == Kind::UINT) &&
						ReadComponent(symbol.value, 0) == length)
						return symbol.name;
				}
			}
			break;
		}
		return std::to_string(length);
	}

	std::string TypeText(GenType* type, bool allow_references)
	{
		if (type->kind == Kind::ARRAY)
			return "[" + LengthText(type->length, allow_references) + "]" + TypeText(type->element, allow_references);
		return type->name;
	}

	// Expressions

	Expr Literal(GenType* type)
	{
		Expr expr;
		expr.literal = true;
		expr.constant = true;
		expr.value.resize(4);
		uint32 bits = 0;
		char text[64];

		if (type->kind == Kind::FLOAT && Below(4) == 0)
		{
			// An integer literal, exactly representable.
			static const uint32 exact[] = { 0, 1, 2, 3, 100, 16777216, 0x40000000, 0x80000000 };
			uint32 integer = Below(2) ? exact[Below(8)] : Below(16777217);
			snprintf(text, sizeof(text), Below(4) ? "%u" : "0x%X", integer);
			bits = FromFloat((float)integer);
			expr.hint_free = false;
		}
		else if (type->kind == Kind::FLOAT)
		{
			static const float interesting[] = {
				0.0f, 0.5f, 1.0f, 2.0f, 0.1f, 1e10f, 1e-45f, 1.17549435e-38f, 3.40282347e38f, 16777217.0f };
			float value = interesting[Below(10)];
			if (Below(3) == 0)
			{
				uint32 random = decisions.Bits() & 0x7FFFFFFF;
				if ((random >> 23) == 0xFF)
					random &= ~(1u << 23); // finite
				value = ToFloat(random);
			}
			snprintf(text, sizeof(text), "%.9g", value);
			if (!strpbrk(text, ".e"))
				memcpy(text + strlen(text), ".0", 3);
			bits = FromFloat(value);
		}
		else
		{
			uint32 max = type->kind == Kind::INT ? INT32_MAX : UINT32_MAX;
			static const uint32 interesting[] = { 0, 1, 2, 7, 255, 65535, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
			uint32 integer = 0;
			switch (Below(3))
			{
			case 0: integer = Below(10); break;
			case 1: integer = interesting[Below(9)]; break;
			case 2: integer = decisions.Bits(); break;
			}
			if (integer > max)
				integer &= max;
			snprintf(text, sizeof(text), Below(4) ? "%u" : "0x%X", integer);
			bits = integer;
			expr.hint_free = type->kind == Kind::INT;
		}
		expr.text = text;
		WriteComponent(expr.value, 0, bits);
		return expr;
	}

	// Symbols holding a value of type somewhere inside them, reachable through indexing, and member access if allowed.
	bool Contains(GenType* outer, GenType* type, bool members)
	{
		if (outer == type)
			return true;
		if (outer->kind == Kind::ARRAY)
			return Contains(outer->element, type, members);
		if (outer->kind == Kind::STRUCT && members)
		{
			for (GenField& field : outer->fields)
			{
				if (Contains(field.type, type, members))
					return true;
			}
		}
		return false;
	}

	// An index into an array of length: constant and in bounds, or wrapped with % so it is whenever it's constant.
	Expr Index(uint32 length, Where where, uint32 depth)
	{
		Expr index;
		if (Below(2) == 0)
		{
			index = Literal(uint_type);
			uint32 value = ReadComponent(index.value, 0) % length;
			index.text = std::to_string(value);
			WriteComponent(index.value, 0, value);
			return index;
		}
		Expr inner = Generate(uint_type, { .constant_only = where.constant_only }, depth + 1);
		index.text = "(" + inner.text + ") % " + std::to_string(length);
		index.constant = inner.constant;
		if (inner.constant)
		{
			index.value.resize(4);
			WriteComponent(index.value, 0, ReadComponent(inner.value, 0) % length);
		}
		return index;
	}

	// a, a[i], a.f, a.f[i].g... of type, starting from a symbol. False if no symbol contains one.
	bool Place(GenType* type, Where where, uint32 depth, Expr& out)
	{
		// The compiler doesn't evaluate member access as a constant.
		bool members = !where.constant_only;
		std::vector<Symbol*> candidates;
		for (Symbol& symbol : symbols)
		{
			if ((symbol.constant || !where.constant_only) && Contains(symbol.type, type, members))
				candidates.push_back(&symbol);
		}
		if (candidates.empty())
			return false;

		Symbol* symbol = candidates[Below((uint32)candidates.size())];
		GenType* current = symbol->type;
		out = {};
		out.text = symbol->name;
		out.constant = symbol->constant && !symbol->value.empty();
		uint32 offset = 0;
		while (current != type)
		{
			if (current->kind == Kind::ARRAY)
			{
				Expr index = Index(current->length, where, depth);
				out.text += "[" + index.text + "]";
				if (out.constant && index.constant)
					offset += ReadComponent(index.value, 0) * current->stride;
				else
					out.constant = false;
				current = current->element;
				continue;
			}
			std::vector<GenField*> fields;
			for (GenField& field : current->fields)
			{
				if (Contains(field.type, type, members))
					fields.push_back(&field);
			}
			GenField* field = fields[Below((uint32)fields.size())];
			out.text += "." + field->name;
			out.constant = false;
			current = field->type;
		}
		if (out.constant)
			out.value.assign(symbol->value.begin() + offset, symbol->value.begin() + offset + type->size);
		return true;
	}

	Expr Unary(GenType* type, Where where, uint32 depth)
	{
		Kind kind = ComponentKind(type);
		const char* ops[3];
		uint32 count = 0;
		ops[count++] = "+";
		if (kind == Kind::INT || kind == Kind::FLOAT)
			ops[count++] = "-";
		if (kind == Kind::INT || kind == Kind::UINT)
			ops[count++] = "~";
		const char* op = ops[Below(count)];

		Expr operand = Generate(type, { .constant_only = where.constant_only }, depth + 1);
		Expr expr;
		expr.text = std::string(op) + "(" + operand.text + ")";
		expr.hint_free = operand.hint_free;
		expr.constant = operand.constant;
		if (expr.constant)
		{
			expr.value = operand.value;
			for (uint32 i = 0; i < type->size / 4; ++i)
			{
				uint32 bits = ReadComponent(operand.value, i);
				if (*op == '-')
					bits = kind == Kind::FLOAT ? FromFloat(-ToFloat(bits)) : 0u - bits;
				else if (*op == '~')
					bits = ~bits;
				WriteComponent(expr.value, i, bits);
			}
		}
		return expr;
	}

	static uint32 Fold(Kind kind, char op, uint32 a, uint32 b)
	{
		if (kind == Kind::FLOAT)
		{
			float x = ToFloat(a);
			float y = ToFloat(b);
			switch (op)
			{
			case '+': return FromFloat(x + y);
			case '-': return FromFloat(x - y);
			case '*': return FromFloat(x * y);
			case '/': return FromFloat(x / y);
			default: return FromFloat(fmodf(x, y));
			}
		}
		switch (op)
		{
		case '+': return a + b;
		case '-': return a - b;
		case '*': return a * b;
		default: break;
		}
		if (kind == Kind::INT)
			return op == '/' ? (uint32)((int32)a / (int32)b) : (uint32)((int32)a % (int32)b);
		return op == '/' ? a / b : a % b;
	}

	Expr Binary(GenType* type, Where where, uint32 depth)
	{
		Kind kind = ComponentKind(type);
		char op = "+-*/%"[Below(5)];
		Expr left = Generate(type, { .constant_only = where.constant_only }, depth + 1);
		Expr right = Generate(type, { .constant_only = where.constant_only }, depth + 1);

		// Division by zero and INT_MIN / -1 are errors when the compiler can evaluate them.
		if (left.constant && right.constant && kind != Kind::FLOAT && (op == '/' || op == '%'))
		{
			for (uint32 i = 0; i < type->size / 4; ++i)
			{
				uint32 a = ReadComponent(left.value, i);
				uint32 b = ReadComponent(right.value, i);
				if (b == 0 || (kind == Kind::INT && a == 0x80000000 && b == 0xFFFFFFFF))
					op = '+';
			}
		}

		Expr expr;
		expr.text = "(" + left.text + " " + op + " " + right.text + ")";
		// Without an expected type, two literals are float if either is written as one, else int. One literal takes
		// the other side's type, and otherwise the right side takes the left's.
		if (left.literal && right.literal)
			expr.hint_free = left.hint_free || right.hint_free;
		else if (left.literal)
			expr.hint_free = right.hint_free;
		else
			expr.hint_free = left.hint_free;

		expr.constant = left.constant && right.constant;
		if (expr.constant)
		{
			expr.value.resize(type->size);
			for (uint32 i = 0; i < type->size / 4; ++i)
				WriteComponent(expr.value, i, Fold(kind, op, ReadComponent(left.value, i), ReadComponent(right.value, i)));
		}
		return expr;
	}

	// floatN(...): its components from scalars and smaller vectors, or one scalar for all of them.
	Expr Constructor(GenType* type, Where where, uint32 depth)
	{
		std::vector<GenType*> parts;
		if (Below(3) == 0)
			parts.push_back(float_type);
		else
		{
			uint32 remaining = type->count;
			while (remaining)
			{
				uint32 part = 1 + Below(remaining);
				parts.push_back(part == 1 ? float_type : vector_types[part]);
				remaining -= part;
			}
		}

		Expr expr;
		expr.text = type->name + "(";
		expr.constant = true;
		Bytes value;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			Expr argument = Generate(parts[i], { .constant_only = where.constant_only }, depth + 1);
			expr.text += (i ? ", " : "") + argument.text;
			expr.constant = expr.constant && argument.constant;
			if (argument.constant)
				value.insert(value.end(), argument.value.begin(), argument.value.end());
		}
		expr.text += ")";
		if (expr.constant)
		{
			if (parts.size() == 1 && parts[0] == float_type)
			{
				for (uint32 i = 1; i < type->count; ++i)
					value.insert(value.end(), value.begin(), value.begin() + 4);
			}
			expr.value = value;
		}
		return expr;
	}

	Expr InitList(GenType* type, Where where, uint32 depth)
	{
		Expr expr;
		expr.hint_free = false;
		uint32 count = type->kind == Kind::ARRAY ? type->length : (uint32)type->fields.size();
		if (count > 256 || type->size > MAX_INIT_LIST_SIZE)
		{
			expr.text = "{ }"; // chaos mode's huge types
			return expr;
		}
		expr.text = "{";
		expr.constant = true;
		expr.value.assign(type->size, 0);
		for (uint32 i = 0; i < count; ++i)
		{
			GenType* element = type->kind == Kind::ARRAY ? type->element : type->fields[i].type;
			uint32 offset = type->kind == Kind::ARRAY ? i * type->stride : type->fields[i].offset;
			Expr value = Generate(element, { .constant_only = where.constant_only, .init_list = true }, depth + 1);
			expr.text += (i ? ", " : " ") + value.text;
			expr.constant = expr.constant && value.constant;
			if (value.constant)
				memcpy(expr.value.data() + offset, value.value.data(), element->size);
		}
		expr.text += count && Below(4) == 0 ? ", }" : " }";
		if (!expr.constant)
			expr.value.clear();
		return expr;
	}

	// An expression of the given type. Choice 0 is always a leaf, so generation ends once the input runs out.
	Expr Generate(GenType* type, Where where, uint32 depth)
	{
		if (chaos && Chance(3))
			return Chaos(where, depth);

		Expr expr;
		bool leaf = depth >= MAX_DEPTH;
		if (IsComposite(type))
		{
			// Only asked for where an init list works, the expected type is known.
			if ((leaf || Below(3)) && Place(type, where, depth, expr))
				return expr;
			return InitList(type, where, depth);
		}

		switch (leaf ? 0 : Below(5))
		{
		case 1:
			if (Place(type, where, depth, expr))
				return expr;
			break;
		case 2: return Unary(type, where, depth);
		case 3: return Binary(type, where, depth);
		case 4:
			if (type->kind == Kind::VECTOR)
				return Constructor(type, where, depth);
			break;
		}
		if (type->kind == Kind::VECTOR)
			return Constructor(type, where, MAX_DEPTH);
		return Literal(type);
	}

	// Something likely wrong, for chaos mode. Never constant, so nothing relies on its value.
	Expr Chaos(Where where, uint32 depth)
	{
		Expr expr;
		expr.hint_free = false;
		if (depth > MAX_DEPTH + 2)
		{
			expr.text = "1";
			return expr;
		}

		auto any = [&]() { return Generate(PickScalarOrVector(), where, depth + 1).text; };
		auto any_symbol = [&]() { return symbols.empty() ? std::string("x") : symbols[Below((uint32)symbols.size())].name; };
		switch (Below(19))
		{
		case 0: expr.text = any(); break; // just the wrong type, usually
		case 1: expr.text = "if " + any() + " { " + any() + " } else " + any(); break;
		case 2: expr.text = Below(2) ? "true" : "false"; break;
		case 3:
		{
			const char* const ops[] = { "==", "!=", "<", ">=", "&&", "||", "<<", ">>", "&", "|", "^", "=", "+=" };
			expr.text = "(" + any() + " " + ops[Below(13)] + " " + any() + ")";
			break;
		}
		case 4: expr.text = (Below(2) ? "!" : "++") + any(); break;
		case 5: expr.text = "(" + any() + ")" + (Below(2) ? "++" : "--"); break;
		case 6: expr.text = "unknown" + std::to_string(Below(4)); break;
		case 7: expr.text = Below(2) ? "{}" : "{ " + any() + ", " + any() + " }"; break;
		case 8: expr.text = "float4(" + any() + ", " + any() + ")"; break;
		case 9:
		{
			const char* const numbers[] = { "2147483648", "4294967296", "16777217", "1e39", "18446744073709551615",
				"0xFFFFFFFFFFFFFFFF", "1e-50", "0.0" };
			expr.text = numbers[Below(8)];
			break;
		}
		case 10: expr.text = TypeText(PickType((uint32)struct_types.size()), true); break; // a type as a value
		case 11:
		{
			// Deep nesting, around the recursion limit.
			uint32 levels = 200 + Below(120);
			const char* const opens[] = { "(", "-(", "{", "[1]", "float4(" };
			const char* const closes[] = { ")", ")", "}", "", ")" };
			uint32 kind = Below(5);
			std::string inner = kind == 3 ? "float" : "1.0";
			expr.text.reserve((size_t)levels * 8);
			for (uint32 i = 0; i < levels; ++i)
				expr.text += opens[kind];
			expr.text += inner;
			for (uint32 i = 0; i < levels; ++i)
				expr.text += closes[kind];
			break;
		}
		case 12:
		{
			// A long chain, deep as a tree but not as recursion in the parser.
			uint32 length = 100 + Below(400);
			expr.text = "1";
			for (uint32 i = 0; i < length; ++i)
				expr.text += Below(2) ? " + 1" : " * 1";
			break;
		}
		case 13:
			if (!symbols.empty())
			{
				Symbol& symbol = symbols[Below((uint32)symbols.size())];
				expr.text = symbol.name + (Below(2) ? ".m" + std::to_string(Below(4)) : "[" + std::to_string(Below(8)) + "]");
				break;
			}
			expr.text = "x";
			break;
		case 14:
			// Intrinsics and enum values used as values.
			expr.text = Below(2) ? "semantic(position)" : (Below(2) ? "location" : "vertex_index");
			break;
		case 15: expr.text = TypeText(PickType((uint32)struct_types.size()), true) + "(" + any() + ")"; break; // int(1), S(1)
		case 16:
		{
			// Arithmetic on whatever the names are: structs, arrays, types, functions.
			const char* const ops[] = { "+", "-", "*", "/", "%" };
			std::string operand = Below(2) ? any_symbol() : any();
			expr.text = "(" + any_symbol() + " " + ops[Below(5)] + " " + operand + ")";
			break;
		}
		case 17: expr.text = "fn" + std::to_string(Below(next_name + 1)); break; // a function as a value
		default: expr.text = "fn" + std::to_string(Below(next_name + 1)) + "(" + any() + ")"; break;
		}
		return expr;
	}

	// Declarations

	void AddSymbol(const std::string& name, GenType* type, const Expr* value)
	{
		Symbol symbol;
		symbol.name = name;
		symbol.type = type;
		symbol.constant = value != nullptr;
		if (value && value->constant && type->size <= MAX_VALUE_SIZE)
			symbol.value = value->value;
		symbols.push_back(std::move(symbol));
	}

	std::string Attribute()
	{
		const char* const attributes[] = { "@semantic(position)", "@semantic(vertex_index)", "@location(0)", "@location(4294967295)",
			"@entry(vertex)", "@entry(fragment)", "@semantic", "@location", "@semantic(location)", "@location(-1)", "@entry(1)", "@1",
			"@position", "@semantic(position, position)", "@location(1.5)", "@unknown(1)", "@entry", "@entry(vertex, fragment)",
			"@entry(position)", "@vertex" };
		return std::string(attributes[Below(sizeof(attributes) / sizeof(attributes[0]))]) + " ";
	}

	std::string Struct(GenType* type, uint32 index)
	{
		std::string text = "struct " + type->name + "\n{\n";
		for (GenField& field : type->fields)
		{
			text += "\t";
			text += IOAttribute(field.semantic, field.location, false);
			if (chaos && Chance(10))
				text += Attribute();
			text += field.name + ": " + TypeText(field.type, false) + ";\n";
		}
		if (chaos && Chance(10))
		{
			// Contains itself, directly or through another struct or an array.
			GenType* other = struct_types[Below((uint32)struct_types.size())];
			text += "\tcycle: " + std::string(Below(2) ? "[2]" : "") + other->name + ";\n";
		}
		(void)index;
		return text + "}\n";
	}

	void MakeStructs()
	{
		uint32 count = Below(4);
		for (uint32 i = 0; i < count; ++i)
		{
			GenType* type = NewType(Kind::STRUCT, NewName("S"), 0, 1);
			type->io = Below(3) == 0;
			uint32 field_count = Below(5);
			uint64 offset = 0;
			for (uint32 k = 0; k < field_count; ++k)
			{
				GenField field;
				field.name = "m" + std::to_string(k);
				if (type->io)
				{
					if (!MakeIOField(type, field))
						break;
				}
				else
					field.type = PickType((uint32)struct_types.size());
				offset = RoundUp(offset, field.type->alignment);
				field.offset = (uint32)offset;
				if (!chaos && offset + field.type->size > UINT32_MAX / 2) // chaos mode wants structs too large
					break;
				offset += field.type->size;
				if (field.type->alignment > type->alignment)
					type->alignment = field.type->alignment;
				type->fields.push_back(field);
			}
			offset = RoundUp(offset, type->alignment);
			type->size = offset > UINT32_MAX ? UINT32_MAX : (uint32)offset;
			if (!chaos && type->size > MAX_TYPE_SIZE)
				type->fields.clear(), type->size = 0, type->alignment = 1;
			if (type->fields.empty())
				type->io = false;
			struct_types.push_back(type);
		}
	}

	std::string Const(const char* indent)
	{
		std::string name = NewName("c");
		GenType* type = PickType((uint32)struct_types.size());
		Expr value;

		if (chaos && Chance(15) && !symbols.empty())
		{
			// Copies of the previous const. Several grow sizes exponentially with the source, one at a time they add up,
			// against both limits on constant sizes.
			static const uint32 copies[] = { 1, 2, 4, 16 };
			uint32 count = copies[Below(4)];
			Symbol& previous = symbols.back();
			GenType* grown = ArrayOf(previous.type, count);
			if (grown)
			{
				type = grown;
				value.text = "{";
				for (uint32 i = 0; i < count; ++i)
					value.text += (i ? ", " : " ") + previous.name;
				value.text += " }";
				value.hint_free = false;
			}
		}
		if (value.text.empty())
			value = Generate(type, { .constant_only = true, .init_list = true }, 0);

		std::string text = std::string(indent) + "const " + name;
		if (!value.hint_free || Below(3))
			text += ": " + TypeText(type, true);
		text += " = " + value.text + ";\n";

		if (value.constant)
			expected.push_back({ name, type, true, value.value });
		AddSymbol(name, type, &value);
		return text;
	}

	// A function's body: statements, then a return of its type.
	std::string Body(GenType* return_type, std::string indent, uint32 depth)
	{
		std::string text = indent + "{\n";
		size_t scope = symbols.size();
		std::string inner = indent + "\t";
		uint32 count = Below(5);
		for (uint32 i = 0; i < count && source_size + text.size() < MAX_SOURCE; ++i)
		{
			switch (Below(depth < 3 ? 6 : 4))
			{
			case 0: text += Return(return_type, inner); break;
			case 1:
			{
				std::string name = NewName("v");
				GenType* type = PickType((uint32)struct_types.size());
				text += inner + name + ": " + TypeText(type, true) + ";\n";
				expected.push_back({ name, type, false, {} });
				AddSymbol(name, type, nullptr);
				break;
			}
			case 2: text += Const(inner.c_str()); break;
			case 3:
				if (chaos)
					text += inner + Chaos({}, 0).text + ";\n";
				break;
			case 4: text += Body(return_type, inner, depth + 1); break;
			case 5:
			{
				// Nested functions only see the module's names.
				std::vector<Symbol> outer = symbols;
				symbols.resize(module_symbols);
				text += Function(inner, depth + 1);
				symbols = outer;
				break;
			}
			}
		}
		text += Return(return_type, inner);
		symbols.resize(scope);
		return text + indent + "}\n";
	}

	std::string Return(GenType* return_type, const std::string& indent)
	{
		if (chaos && Chance(10))
			return indent + (return_type ? "return;\n" : "return " + Chaos({}, 0).text + ";\n");
		if (!return_type)
			return indent + "return;\n";
		return indent + "return " + Generate(return_type, { .init_list = true }, 0).text + ";\n";
	}

	// Shader interface

	// How to write a location: a literal, a sum, or a uint const that has the value.
	std::string LocationText(uint32 location, bool allow_references)
	{
		switch (Below(4))
		{
		case 1:
		{
			char text[16];
			snprintf(text, sizeof(text), "0x%X", location);
			return text;
		}
		case 2:
		{
			uint32 left = Below(location + 1);
			return std::to_string(left) + " + " + std::to_string(location - left);
		}
		case 3:
			if (allow_references)
			{
				for (size_t i = symbols.size(); i-- > 0;)
				{
					Symbol& symbol = symbols[i];
					if (symbol.constant && symbol.value.size() == 4 && symbol.type->kind == Kind::UINT &&
						ReadComponent(symbol.value, 0) == location)
						return symbol.name;
				}
			}
			break;
		}
		return std::to_string(location);
	}

	// The attribute of a semantic or location, or nothing if it has neither.
	std::string IOAttribute(int32 semantic, int32 location, bool allow_references)
	{
		if (semantic >= 0)
			return "@semantic(" + std::string(SEMANTIC_NAMES[semantic]) + ") ";
		if (location >= 0)
			return "@location(" + LocationText((uint32)location, allow_references) + ") ";
		return "";
	}

	// A random location that isn't in used, or -1 if there's none.
	int32 FreeLocation(uint32 used)
	{
		uint32 free = 0;
		for (uint32 i = 0; i < LOCATION_COUNT; ++i)
			free += !(used & (1u << i));
		if (!free)
			return -1;
		uint32 pick = Below(free);
		for (uint32 i = 0; i < LOCATION_COUNT; ++i)
		{
			if (!(used & (1u << i)) && pick-- == 0)
				return (int32)i;
		}
		return -1;
	}

	// A random set bit of mask, which isn't 0.
	uint32 PickBit(uint32 mask)
	{
		std::vector<uint32> bits;
		for (uint32 i = 0; i < 32; ++i)
		{
			if (mask & (1u << i))
				bits.push_back(i);
		}
		return bits[Below((uint32)bits.size())];
	}

	// An IO struct declared earlier whose locations and semantics don't clash with used ones, and whose semantics are
	// all allowed and include required. nullptr if there's none.
	GenType* PickIOStruct(uint32 used_locations, uint32 used_semantics, uint32 allowed, uint32 required, uint32 max_struct)
	{
		std::vector<GenType*> candidates;
		for (uint32 i = 0; i < max_struct; ++i)
		{
			GenType* type = struct_types[i];
			if (!type->io)
				continue;
			bool clashes = (type->locations & used_locations) || (type->semantics & used_semantics) ||
						   (type->semantics & ~allowed) || (type->semantics & required) != required;
			if (!clashes || (chaos && Chance(20)))
				candidates.push_back(type);
		}
		return candidates.empty() ? nullptr : candidates[Below((uint32)candidates.size())];
	}

	// A field of an IO struct: a leaf with a semantic or location, or an earlier IO struct, none of them used in the
	// struct yet. False if there are no locations left.
	bool MakeIOField(GenType* type, GenField& field)
	{
		if (chaos && Chance(5))
		{
			field.type = PickType((uint32)struct_types.size()); // without a semantic or location
			return true;
		}
		switch (Below(4))
		{
		case 0:
			if (GenType* nested = PickIOStruct(type->locations, type->semantics, VERTEX_INDEX_BIT | POSITION_BIT, 0,
					(uint32)struct_types.size()))
			{
				field.type = nested;
				type->locations |= nested->locations;
				type->semantics |= nested->semantics;
				return true;
			}
			break;
		case 1:
		{
			uint32 free = (VERTEX_INDEX_BIT | POSITION_BIT) & ~type->semantics;
			if (chaos && Chance(20))
				free = VERTEX_INDEX_BIT | POSITION_BIT;
			if (!free)
				break;
			field.semantic = (int32)PickBit(free);
			field.type = field.semantic == (int32)Semantic::POSITION ? vector_types[4] : uint_type;
			type->semantics |= 1u << field.semantic;
			return true;
		}
		}
		field.location = chaos && Chance(20) ? (int32)Below(4) : FreeLocation(type->locations);
		if (field.location < 0)
			return false;
		field.type = PickScalarOrVector();
		type->locations |= 1u << field.location;
		return true;
	}

	// The semantics an entry point of stage (1: vertex, 2: fragment) can have as inputs or outputs.
	static uint32 AllowedSemantics(uint32 stage, bool input)
	{
		if (stage == 1)
			return input ? VERTEX_INDEX_BIT : POSITION_BIT;
		return input ? POSITION_BIT : 0;
	}

	// The lines ShaderInterfaceToString prints for a value of type with the semantic or location.
	void ExpectIO(std::string& lines, bool input, GenType* type, int32 semantic, int32 location, std::vector<uint32>& path)
	{
		if (type->kind == Kind::STRUCT)
		{
			for (uint32 i = 0; i < type->fields.size(); ++i)
			{
				GenField& field = type->fields[i];
				path.push_back(i);
				ExpectIO(lines, input, field.type, field.semantic, field.location, path);
				path.pop_back();
			}
			return;
		}
		lines += input ? "  input " : "  output ";
		if (semantic >= 0)
			lines += "semantic(" + std::string(SEMANTIC_NAMES[semantic]) + ")";
		else
			lines += "location(" + std::to_string(location) + ")";
		lines += " " + type->name + " [";
		for (size_t i = 0; i < path.size(); ++i)
			lines += (i ? ", " : "") + std::to_string(path[i]);
		lines += "]\n";
	}

	// An entry point's input or output: an IO struct, or a leaf with a semantic or location that isn't used yet.
	// nullptr if there's nothing left to use.
	GenType* PickIO(uint32 stage, bool input, uint32& used_locations, uint32& used_semantics, int32& semantic, int32& location)
	{
		semantic = -1;
		location = -1;
		uint32 allowed = AllowedSemantics(stage, input);
		if (chaos && Chance(15))
		{
			// Anything, with any semantic or location or none.
			switch (Below(3))
			{
			case 0: semantic = (int32)Below(2); break;
			case 1: location = (int32)Below(LOCATION_COUNT + 2); break;
			}
			return PickType((uint32)struct_types.size());
		}
		switch (Below(3))
		{
		case 0:
			if (GenType* type = PickIOStruct(used_locations, used_semantics, allowed, 0, (uint32)struct_types.size()))
			{
				used_locations |= type->locations;
				used_semantics |= type->semantics;
				return type;
			}
			break;
		case 1:
			if (uint32 free = allowed & ~used_semantics)
			{
				semantic = (int32)PickBit(free);
				used_semantics |= 1u << semantic;
				return semantic == (int32)Semantic::POSITION ? vector_types[4] : uint_type;
			}
			break;
		}
		location = FreeLocation(used_locations);
		if (location < 0)
			return nullptr;
		used_locations |= 1u << location;
		return PickScalarOrVector();
	}

	// A vertex entry point's return value, which has to include position.
	GenType* PickVertexOutput(int32& semantic)
	{
		semantic = -1;
		if (Below(2))
		{
			if (GenType* type = PickIOStruct(0, 0, POSITION_BIT, POSITION_BIT, (uint32)struct_types.size()))
				return type;
		}
		semantic = (int32)Semantic::POSITION;
		return vector_types[4];
	}

	size_t module_symbols = 0;

	std::string Function(const std::string& indent, uint32 depth)
	{
		std::string name = NewName("fn");
		std::string text = indent;
		uint32 stage = depth && !(chaos && Chance(5)) ? 0 : Below(3); // 1: vertex, 2: fragment. Nested only in chaos mode
		if (stage)
			text += stage == 1 ? "@entry(vertex)\n" + indent : "@entry(fragment)\n" + indent;
		if (chaos && Chance(10))
			text += Attribute();
		text += "function " + name + "(";

		std::string interface_lines;
		std::vector<uint32> path;
		size_t scope = symbols.size();
		uint32 parameter_count = Below(4);
		uint32 used_locations = 0;
		uint32 used_semantics = 0;
		for (uint32 i = 0; i < parameter_count; ++i)
		{
			GenType* type = nullptr;
			std::string attribute;
			if (stage)
			{
				int32 semantic = -1;
				int32 location = -1;
				type = PickIO(stage, true, used_locations, used_semantics, semantic, location);
				if (!type)
					break;
				attribute = IOAttribute(semantic, location, true);
				path.push_back(i);
				ExpectIO(interface_lines, true, type, semantic, location, path);
				path.pop_back();
			}
			else
				type = PickType((uint32)struct_types.size());

			std::string parameter = NewName("p");
			if (i)
				text += ", ";
			text += attribute;
			if (chaos && Chance(10))
				text += Attribute();
			text += parameter + ": " + TypeText(type, true);
			if (chaos && Chance(10))
				text += " = " + Chaos({}, 0).text;
			expected.push_back({ parameter, type, false, {} });
			AddSymbol(parameter, type, nullptr);
		}
		text += ")";

		GenType* return_type = nullptr;
		if (stage)
		{
			int32 semantic = -1;
			int32 location = -1;
			if (stage == 1 && !(chaos && Chance(15)))
				return_type = PickVertexOutput(semantic);
			else if (Below(3))
			{
				// Fragment outputs are color targets, of which there are 8: the rest count as used.
				uint32 output_locations = stage == 2 ? ~0xFFu : 0;
				uint32 output_semantics = 0;
				return_type = PickIO(stage, false, output_locations, output_semantics, semantic, location);
			}
			if (return_type)
			{
				text += ": " + IOAttribute(semantic, location, true) + TypeText(return_type, true);
				ExpectIO(interface_lines, false, return_type, semantic, location, path);
			}
			else if (Below(2))
				text += ": void";
			if (!depth)
				expected_interface += (stage == 1 ? "vertex " : "fragment ") + name + "\n" + interface_lines;
		}
		else if (Below(3))
		{
			return_type = PickType((uint32)struct_types.size());
			text += ": " + TypeText(return_type, true);
		}
		else if (Below(2))
			text += ": void";
		text += "\n";

		text += Body(return_type, indent, depth);
		symbols.resize(scope);
		return text;
	}

	std::string Program()
	{
		InitTypes();
		MakeStructs();

		// Structs can be used before they're declared, so they go anywhere among the consts.
		std::vector<std::string> declarations;
		uint32 const_count = Below(8);
		for (uint32 i = 0; i < const_count && source_size < MAX_SOURCE; ++i)
		{
			declarations.push_back(Const(""));
			source_size += declarations.back().size();
		}
		for (uint32 i = 0; i < struct_types.size(); ++i)
		{
			size_t position = Below((uint32)declarations.size() + 1);
			declarations.insert(declarations.begin() + (ptrdiff_t)position, Struct(struct_types[i], i));
		}

		module_symbols = symbols.size();
		uint32 function_count = Below(4);
		for (uint32 i = 0; i < function_count && source_size < MAX_SOURCE; ++i)
		{
			declarations.push_back(Function("", 0));
			source_size += declarations.back().size();
		}

		std::string source;
		for (const std::string& declaration : declarations)
			source += declaration + "\n";
		return source;
	}
};

// Valid mode: the program compiled without errors to what the generator expected.
void CheckExpected(Fuzz::Compilation& compilation, void* user)
{
	Generator& generator = *(Generator*)user;
	if (compilation.failed_stage != Fuzz::Stage::DONE)
		Fuzz::Fail("a valid program failed: %s", compilation.errors[0]->message.CString());

	std::vector<Node*> declarations;
	std::vector<Node*> stack = { compilation.module };
	while (!stack.empty())
	{
		Node* node = stack.back();
		stack.pop_back();
		if (node->node_type == NodeType::CONST || node->node_type == NodeType::PARAMETER ||
			node->node_type == NodeType::VARIABLE || node->node_type == NodeType::STRUCT)
			declarations.push_back(node);
		for (Node* child = node->child; child; child = child->next)
			stack.push_back(child);
	}
	Arena* arena = compilation.intermediate_arena;
	auto find = [&](const std::string& name) -> Node* {
		for (Node* node : declarations)
		{
			if (GetAtomString(node->name, arena) == StringView(name.c_str()))
				return node;
		}
		Fuzz::Fail("no declaration named %s", name.c_str());
	};

	for (Expected& expected : generator.expected)
	{
		Node* node = find(expected.name);
		ZTStringView type = node->type ? TypeToString(node->type, arena) : "(none)";
		if (!(type == StringView(expected.type->name.c_str())))
			Fuzz::Fail("%s has type %s, expected %s", expected.name.c_str(), type.CString(), expected.type->name.c_str());
		if (!expected.has_value)
			continue;
		Constant* constant = FindChild(node, Usage::VALUE)->constant; // folded
		if (constant->bytes.count != expected.value.size() ||
			(constant->bytes.count && memcmp(constant->bytes.data, expected.value.data(), constant->bytes.count) != 0))
		{
			std::string got;
			std::string want;
			char hex[4];
			for (uint32 i = 0; i < constant->bytes.count; ++i)
				snprintf(hex, sizeof(hex), "%02X", constant->bytes.data[i]), got += hex;
			for (uint8 byte : expected.value)
				snprintf(hex, sizeof(hex), "%02X", byte), want += hex;
			Fuzz::Fail("%s's value is\n    %s\nexpected\n    %s", expected.name.c_str(), got.c_str(), want.c_str());
		}
	}

	for (GenType* expected : generator.struct_types)
	{
		StructType* type = (StructType*)find(expected->name)->type;
		bool same = type->size == expected->size && type->alignment == expected->alignment &&
					type->fields.count == expected->fields.size();
		for (uint32 i = 0; same && i < type->fields.count; ++i)
			same = type->fields[i].offset == expected->fields[i].offset;
		if (!same)
			Fuzz::Fail("%s's layout differs: size %u alignment %u, expected size %u alignment %u", expected->name.c_str(),
				type->size, type->alignment, expected->size, expected->alignment);
	}

	ZTStringView shader_interface = ShaderInterfaceToString(compilation.shader_interface, arena);
	if (!(shader_interface == StringView(generator.expected_interface.c_str())))
		Fuzz::Fail("the shader interface is\n%sexpected\n%s", shader_interface.CString(), generator.expected_interface.c_str());
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static bool initialized = (Fuzz::InitFuzzing(), true);
	static bool print = getenv("EVA_FUZZ_PRINT") != nullptr;
	(void)initialized;

	Generator generator;
	generator.decisions = { .data = data, .size = size };
	generator.chaos = generator.decisions.Byte() & 1;
	std::string source = generator.Program();
	if (print)
		printf("---- %s program ----\n%s", generator.chaos ? "chaos" : "valid", source.c_str());

	Fuzz::CheckSource(ZTStringView(source.c_str()), generator.chaos ? nullptr : CheckExpected, &generator);
	return 0;
}
