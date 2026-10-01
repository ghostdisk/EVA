#pragma once
#include <EVA/Core/Common.hpp>
#include <new>
#include <stdarg.h>

namespace EVA
{

// Fixed-capacity bump allocator. Panics when out of memory, allocations never return nullptr.
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

// Copies the string into the arena, zero terminated.
ZTStringView InternString(Arena* arena, StringView string);

// printf into the arena. Returns an empty string if formatting fails.
ZTStringView avprintf(Arena* arena, const char* format, va_list args);
ZTStringView aprintf(Arena* arena, const char* format, ...);

}
