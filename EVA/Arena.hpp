#pragma once
#include <EVA/Common.hpp>

#define NUM_FRAME_ARENAS 3

struct Arena {
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
char* ArenaInternCString(Arena* arena, const char* cstring, int len = -1);
char* ArenaPrintf(Arena* arena, const char* fmt, ...);
char* ArenaVprintf(Arena* arena, const char* fmt, va_list args);
void ArenaReset(Arena* arena);
void RotateFrameArenas();

extern Arena* FrameArenas[NUM_FRAME_ARENAS];
extern Arena* FrameArena; // TODO: Add a scratch arena and check most use-cases - a lot of them should be scratchbuffers instead.