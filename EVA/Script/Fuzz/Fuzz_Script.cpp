#include <EVA/Script/Fuzz/FuzzCommon.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>

// Fuzzes the compiler with source text. libFuzzer's byte mutations, steered by Script.dict, are good at the lexer and
// the edges of the parser. The token mutator below adds structural edits byte mutations rarely get right (moving,
// repeating and nesting whole tokens), so more inputs get past the parser into the resolver and typer.

using namespace EVA;
using namespace EVA::Script;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static bool initialized = (Fuzz::InitFuzzing(), true);
	(void)initialized;

	// Exactly size + 1 bytes, so ASan catches the lexer reading past the terminator. A '\0' in the input ends the
	// source early, as it would for the engine.
	char* source = (char*)malloc(size + 1);
	memcpy(source, data, size);
	source[size] = '\0';
	Fuzz::CheckSource(ZTStringView(source));
	free(source);
	return 0;
}

#ifdef EVA_FUZZ

extern "C" size_t LLVMFuzzerMutate(uint8_t* data, size_t size, size_t max_size);

namespace
{

struct Span
{
	size_t start;
	size_t length;
};

const char* const multi_char_operators[] = {
	"<<=", ">>=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "++", "--", "<<", ">>", "==", "!=", "<=", ">=", "&&", "||",
};

// Tokens and snippets to insert. Mostly what the language has, plus a few things it doesn't.
const char* const fragments[] = {
	"const", "struct", "function", "if", "else", "return", "true", "false",
	"void", "int", "uint", "float", "float2", "float3", "float4",
	"builtin", "location", "vertex", "fragment", "vertex_index", "position",
	"@builtin(position)", "@builtin(vertex_index)", "@location(0)", "@vertex", "@fragment",
	";", ",", ":", "=", ".", "@", "(", ")", "[", "]", "{", "}", "+", "-", "*", "/", "%", "~", "!", "<", ">",
	"&", "|", "^", "<<", ">>", "==", "!=", "&&", "||", "++", "--", "+=", "<<=",
	"0", "1", "2", "3", "0.5", "1.0", "-1", "0x7FFFFFFF", "0x80000000", "0xFFFFFFFF", "2147483647", "2147483648",
	"4294967295", "4294967296", "16777217", "1e38", "1e39", "3.4028235e38", "1e-45", "18446744073709551615",
	"[3]", "[4]float4", "[1]", "[0]", "[65536]", "[4294967295]",
	"a", "b", "x", "S", "f",
	"const a = 1;", "const v: float4 = float4(1.0);", "struct S { a: float; b: float2; }", "function f(): float { return 1.0; }",
	"x: float;", "{ }", "return;", "/* */", "//\n",
};

bool IsWordChar(uint8 ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

bool IsSpace(uint8 ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

// Splits the input into tokens roughly the way the lexer does, never failing. Whitespace and comments are left out:
// they're dropped when tokens are put back together with single spaces.
std::vector<Span> Tokenize(const uint8* data, size_t size)
{
	std::vector<Span> tokens;
	size_t i = 0;
	while (i < size)
	{
		uint8 ch = data[i];
		size_t start = i;
		if (IsSpace(ch))
		{
			i++;
			continue;
		}
		if (ch == '/' && i + 1 < size && data[i + 1] == '/')
		{
			while (i < size && data[i] != '\n')
				i++;
			continue;
		}
		if (ch == '/' && i + 1 < size && data[i + 1] == '*')
		{
			i += 2;
			while (i + 1 < size && !(data[i] == '*' && data[i + 1] == '/'))
				i++;
			i = i + 2 < size ? i + 2 : size;
			continue;
		}
		if (IsWordChar(ch) || (ch == '.' && i + 1 < size && data[i + 1] >= '0' && data[i + 1] <= '9'))
		{
			i++;
			while (i < size && (IsWordChar(data[i]) || data[i] == '.' ||
								   ((data[i] == '+' || data[i] == '-') && (data[i - 1] == 'e' || data[i - 1] == 'E'))))
				i++;
		}
		else
		{
			size_t length = 1;
			for (const char* op : multi_char_operators)
			{
				size_t op_length = strlen(op);
				if (i + op_length <= size && memcmp(data + i, op, op_length) == 0)
				{
					length = op_length;
					break;
				}
			}
			i += length;
		}
		tokens.push_back({ start, i - start });
	}
	return tokens;
}

struct Piece
{
	const uint8* data;
	size_t length;
};

Piece TokenPiece(const uint8* data, Span span)
{
	return { data + span.start, span.length };
}

Piece FragmentPiece(const char* fragment)
{
	return { (const uint8*)fragment, strlen(fragment) };
}

// Applies one random token edit to pieces.
void MutatePieces(std::vector<Piece>& pieces, std::minstd_rand& random)
{
	auto below = [&](size_t n) { return n ? random() % n : 0; };
	const size_t fragment_count = sizeof(fragments) / sizeof(fragments[0]);
	Piece fragment = FragmentPiece(fragments[below(fragment_count)]);

	if (pieces.empty())
	{
		pieces.push_back(fragment);
		return;
	}
	size_t i = below(pieces.size());
	size_t j = below(pieces.size());
	switch (random() % 8)
	{
	case 0: pieces.erase(pieces.begin() + i); break;
	case 1: pieces.insert(pieces.begin() + i, pieces[i]); break;
	case 2: std::swap(pieces[i], pieces[j]); break;
	case 3: pieces[i] = fragment; break;
	case 4: pieces.insert(pieces.begin() + i, fragment); break;
	case 5:
	{
		// Repeat a run of tokens somewhere else, which builds nesting: "(" "a" -> "(" "(" "a".
		size_t length = 1 + below(pieces.size() - i < 16 ? pieces.size() - i : 16);
		std::vector<Piece> run(pieces.begin() + i, pieces.begin() + i + length);
		size_t times = 1 + below(random() % 4 ? 2 : 64);
		for (size_t k = 0; k < times; ++k)
			pieces.insert(pieces.begin() + j, run.begin(), run.end());
		break;
	}
	case 6:
	{
		// Reuse a name from elsewhere in the input.
		for (size_t tries = 0; tries < 8; ++tries)
		{
			size_t k = below(pieces.size());
			if (pieces[k].length && IsWordChar(pieces[k].data[0]) && !(pieces[k].data[0] >= '0' && pieces[k].data[0] <= '9'))
			{
				pieces[i] = pieces[k];
				break;
			}
		}
		break;
	}
	case 7:
	{
		// Wrap a run of tokens in brackets.
		static const char* const brackets[][2] = { { "(", ")" }, { "[", "]" }, { "{", "}" }, { "-(", ")" }, { "{", ";}" } };
		const char* const* pair = brackets[below(5)];
		size_t end = i + 1 + below(pieces.size() - i);
		pieces.insert(pieces.begin() + end, FragmentPiece(pair[1]));
		pieces.insert(pieces.begin() + i, FragmentPiece(pair[0]));
		break;
	}
	}
}

// Joins the pieces with spaces into out. Returns the size, or 0 if it doesn't fit.
size_t Join(const std::vector<Piece>& pieces, uint8* out, size_t max_size)
{
	size_t size = 0;
	for (const Piece& piece : pieces)
	{
		if (size + piece.length + 1 > max_size)
			return 0;
		memcpy(out + size, piece.data, piece.length);
		size += piece.length;
		out[size++] = ' ';
	}
	return size;
}

}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size, size_t max_size, unsigned int seed)
{
	std::minstd_rand random(seed);
	if (random() % 2)
		return LLVMFuzzerMutate(data, size, max_size);

	std::vector<uint8> input(data, data + size);
	std::vector<Piece> pieces;
	for (Span span : Tokenize(input.data(), input.size()))
		pieces.push_back(TokenPiece(input.data(), span));

	size_t edits = 1 + random() % 3;
	for (size_t i = 0; i < edits; ++i)
		MutatePieces(pieces, random);

	size_t new_size = Join(pieces, data, max_size);
	if (!new_size)
		return LLVMFuzzerMutate(data, size, max_size);
	return new_size;
}

// Splices the token streams: a prefix of one input with a suffix of the other, cut at token boundaries.
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1, size_t size1, const uint8_t* data2, size_t size2,
	uint8_t* out, size_t max_out_size, unsigned int seed)
{
	std::minstd_rand random(seed);
	std::vector<Span> tokens1 = Tokenize(data1, size1);
	std::vector<Span> tokens2 = Tokenize(data2, size2);
	size_t cut1 = tokens1.empty() ? 0 : random() % (tokens1.size() + 1);
	size_t cut2 = tokens2.empty() ? 0 : random() % (tokens2.size() + 1);

	std::vector<Piece> pieces;
	for (size_t i = 0; i < cut1; ++i)
		pieces.push_back(TokenPiece(data1, tokens1[i]));
	for (size_t i = cut2; i < tokens2.size(); ++i)
		pieces.push_back(TokenPiece(data2, tokens2[i]));
	return Join(pieces, out, max_out_size);
}

#endif
