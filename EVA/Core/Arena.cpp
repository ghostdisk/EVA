#include <EVA/Core/Arena.hpp>
#include <stdlib.h>

namespace EVA
{

static void OutOfMemory()
{
	fprintf(stderr, "out of memory\n");
	exit(1);
}

void Arena::AlignHead(size_t alignment)
{
	assert(alignment && (alignment & (alignment - 1)) == 0);
	uintptr_t aligned = ((uintptr_t)head + alignment - 1) & ~(uintptr_t)(alignment - 1);
	head = aligned > (uintptr_t)end ? end : (uint8*)aligned;
}

void* Arena::Allocate(size_t size)
{
	if (size > (size_t)(end - head))
		OutOfMemory();
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
		OutOfMemory();

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

}
