#pragma once
#include <EVA/Core/Basic.hpp>

struct Hash {
	U32 hash = 0;

	Hash() {}
	Hash(U32 id);
	Hash(const void* ptr);
	Hash(const void* bytes, size_t size);
	Hash(const char* string);

	inline operator U32() const { return hash; }
};

struct HashStack {
	Vector<U32> stack;

	void Reset();
	U32 Push(Hash hash);
	void Pop();
};

U32 HashU32(U32 value, U32 seed = 0);
U32 HashI32(I32 value, U32 seed = 0);
U32 HashBytes(const void* key, int len, U32 seed = 0);
