#pragma once
#include <EVA/Common.hpp>


struct Arena
{
	U8* begin = nullptr;
	U8* end   = nullptr;
	U8* head  = nullptr;
};

void ArenaInitialize();
Arena* ArenaCreate();
void ArenaDestroy(Arena* arena);
void* ArenaAllocate(Arena* arena, size_t size);
void* ArenaAllocate(Arena* arena, size_t size, size_t alignment);
void ArenaAlignHead(Arena* arena, size_t alignment);
char* ArenaInternCString(Arena* arena, const char* cstring);
void ArenaReset(Arena* arena);

extern Arena* FrameArena;