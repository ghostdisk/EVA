#pragma once
#include <EVA/Core/Common.hpp>

#define NUM_FRAME_ARENAS 3

struct String;
struct ZTString;

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
void ArenaReset(Arena* arena);
void RotateFrameArenas();

char* ArenaInternCString(Arena* arena, const char* cstring, int len = -1); // TODO: Remove this
ZTString Fmt(Arena* arena, const char* fmt, ...);
ZTString Vfmt(Arena* arena, const char* fmt, va_list args);

extern Arena* FrameArenas[NUM_FRAME_ARENAS];
extern Arena* FrameArena; // TODO: Add a scratch arena and check most use-cases - a lot of them should be scratchbuffers instead.