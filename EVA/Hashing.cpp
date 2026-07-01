#include <EVA/Hashing.hpp>

// https://gist.github.com/kevinmoran/0198d8e9de0da7057abe8b8b34d50f86
U32 HashU32(U32 value, U32 seed)
{
	constexpr U32 SQ5_BIT_NOISE1 = 0xd2a80a3f;
	constexpr U32 SQ5_BIT_NOISE2 = 0xa884f197;
	constexpr U32 SQ5_BIT_NOISE3 = 0x6C736F4B;
	constexpr U32 SQ5_BIT_NOISE4 = 0xB79F3ABB;
	constexpr U32 SQ5_BIT_NOISE5 = 0x1b56c4f5;

	value *= SQ5_BIT_NOISE1;
	value += seed;
	value ^= (value >> 9);
	value += SQ5_BIT_NOISE2;
	value ^= (value >> 11);
	value *= SQ5_BIT_NOISE3;
	value ^= (value >> 13);
	value += SQ5_BIT_NOISE4;
	value ^= (value >> 15);
	value *= SQ5_BIT_NOISE5;
	value ^= (value >> 17);
	return value;
}

U32 HashI32(I32 value, U32 seed)
{
	return HashU32(value, seed);
}

// https://github.com/abrandoned/murmur2/blob/master/MurmurHash2.c
U32 HashBytes(const void * key, int len, U32 seed)
{
	const U32 m = 0x5bd1e995;
	const int r = 24;

	U32 h = seed ^ len;

	const unsigned char * data = (const unsigned char *)key;
	while(len >= 4)
	{
		U32 k = *(U32*)data;
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		data += 4;
		len -= 4;
	}

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
} 

void HashStack::Reset()
{
	stack = { 1 };
}

Hash::Hash(U32 id)
{
	hash = HashU32(id);
}

Hash::Hash(const void* ptr)
{
	uintptr_t num = (uintptr_t)ptr;
	hash = HashU32((num & 0xFFFFFFFF) | (num >> 32));
}

Hash::Hash(const char* string)
{
	hash = HashBytes(string, strlen(string));
}

Hash::Hash(const void* bytes, size_t len)
{
	hash = HashBytes(bytes, len);
}

U32 HashStack::Push(Hash hash)
{
	stack.push_back(Hash(hash.hash ^ stack.back()));
	return stack.back();
}

void HashStack::Pop()
{
	stack.pop_back();
}
