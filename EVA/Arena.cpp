#include <EVA/Arena.hpp>
#include <string.h>

#define ARENA_SIZE (1 << 20u) // 1MB

Arena* FrameArenas[NUM_FRAME_ARENAS] = {};
Arena* FrameArena = nullptr;

void ArenaInitialize()
{
	for (int i = 0; i < NUM_FRAME_ARENAS; i++)
	{
		FrameArenas[i] = ArenaCreate();
	}
}

void RotateFrameArenas()
{
	static int k = 0;
	k++;
	if (k >= NUM_FRAME_ARENAS) k = 0;

	FrameArena = FrameArenas[k];
	ArenaReset(FrameArena);
}

Arena* ArenaCreate()
{
	Arena* arena = new Arena();
	arena->begin = (U8*)malloc(ARENA_SIZE);
	arena->head = arena->begin;
	arena->end = arena->begin + ARENA_SIZE;
	return arena;
}

void ArenaDestroy(Arena* arena)
{
	free(arena->begin);
	delete arena;
}

void* ArenaAllocate(Arena* arena, size_t size)
{
	U8* ptr = arena->head;
	arena->head += size;
	if (arena->head > arena->end)
	{
		Fatal("ArenaAllocate: out of memory");
	}
	return ptr;

}

void* ArenaAllocate(Arena* arena, size_t size, size_t alignment)
{
	ArenaAlignHead(arena, alignment);
	return ArenaAllocate(arena, size);
}

void ArenaAlignHead(Arena* arena, size_t alignment)
{
	uintptr_t head = (uintptr_t)arena->head;
	uintptr_t mask = ~(uintptr_t)alignment;
	head = (head + mask) & (~mask);
	arena->head = (U8*)head;
}

char* ArenaInternCString(Arena* arena, const char* cstring)
{
	size_t len = strlen(cstring);
	char* copy = (char*)ArenaAllocate(arena, len + 1);
	memcpy(copy, cstring, len);
	copy[len] = '\0';
	return copy;
}

void ArenaReset(Arena* arena)
{
	arena->head = arena->begin;
}