#pragma once
#include <EVA/Core/Common.hpp>
#include <new>

namespace EVA
{

// Fixed-capacity bump allocator. Exits the process when out of memory, allocations never return nullptr.
struct Arena
{
	uint8* begin = nullptr;
	uint8* end = nullptr;
	uint8* head = nullptr;

	void AlignHead(size_t alignment);
	void* Allocate(size_t size);
	void* Allocate(size_t size, size_t alignment);

	template <typename T>
	T* New()
	{
		return new (Allocate(sizeof(T), alignof(T))) T();
	}
};

Arena* CreateArena(size_t capacity);
void DestroyArena(Arena* arena);

}
