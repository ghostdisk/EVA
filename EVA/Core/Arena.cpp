#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Panic.hpp>
#include <stdlib.h>

namespace EVA
{

void Arena::AlignHead(size_t alignment)
{
	assert(alignment && (alignment & (alignment - 1)) == 0);
	uintptr_t aligned = ((uintptr_t)head + alignment - 1) & ~(uintptr_t)(alignment - 1);
	head = aligned > (uintptr_t)end ? end : (uint8*)aligned;
}

void* Arena::Allocate(size_t size)
{
	if (size > (size_t)(end - head))
		Panic("arena out of memory: %zu bytes requested, %zu left", size, (size_t)(end - head));
	void* memory = head;
	head += size;
	return memory;
}

void* Arena::Allocate(size_t size, size_t alignment)
{
	AlignHead(alignment);
	return Allocate(size);
}

Arena* CreateArena(size_t capacity)
{
	uint8* memory = (uint8*)malloc(sizeof(Arena) + capacity);
	if (!memory)
		Panic("out of memory: failed to allocate a %zu byte arena", capacity);

	Arena* arena = new (memory) Arena();
	arena->begin = memory + sizeof(Arena);
	arena->end = arena->begin + capacity;
	arena->head = arena->begin;
	return arena;
}

void DestroyArena(Arena* arena)
{
	free(arena);
}

ZTStringView InternString(Arena* arena, StringView string)
{
	char* copy = (char*)arena->Allocate(string.length + 1, 1);
	if (string.length)
		memcpy(copy, string.data, string.length);
	copy[string.length] = '\0';

	// Set directly rather than via strlen, which would stop early at a '\0' inside the string.
	ZTStringView result;
	result.data = (uint8*)copy;
	result.length = string.length;
	return result;
}

ZTStringView avprintf(Arena* arena, const char* format, va_list args)
{
	// Measure first, args is consumed by each vsnprintf so measure with a copy.
	va_list measure_args;
	va_copy(measure_args, args);
	int length = vsnprintf(nullptr, 0, format, measure_args);
	va_end(measure_args);
	if (length < 0)
		return {};

	char* text = (char*)arena->Allocate((size_t)length + 1, 1);
	vsnprintf(text, (size_t)length + 1, format, args);

	ZTStringView result;
	result.data = (uint8*)text;
	result.length = (size_t)length;
	return result;
}

ZTStringView aprintf(Arena* arena, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	ZTStringView result = avprintf(arena, format, args);
	va_end(args);
	return result;
}

}
