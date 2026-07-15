#pragma once
#include <EVA/Core/Common.hpp>

#define NUM_FRAME_ARENAS 3

class Arena;
struct String;
struct ZTString;

// later on we'll probably add multiple blocks for arenas, and this will keep track of the block we're resetting to as well,
// that's why we're not just working with head directly.
struct ArenaResetPoint {
	U8*    head  = nullptr;
};

class Arena {
public:
	U8* m_begin = nullptr;
	U8* m_end   = nullptr;
	U8* m_head  = nullptr;

	static Arena* Create();
	static void RotateFrameArenas();

	void Destroy();
	void* Allocate(size_t size);
	void* Allocate(size_t size, size_t alignment);
	void AlignHead(size_t alignment);

	ZTString Fmt(const char* fmt, ...);
	ZTString Vfmt(const char* fmt, va_list args);

	ArenaResetPoint GetResetPoint();
	void Reset();
	void Reset(ArenaResetPoint resetPoint);
};

class ScratchArena {
	ArenaResetPoint m_resetPoint;
	Arena* m_arena;
public:
	ScratchArena();
	~ScratchArena();
	operator Arena*();
	Arena* operator->();
	Arena* operator*();
};

void ArenaInitialize();

extern Arena* g_frameArenas[NUM_FRAME_ARENAS];
extern Arena* g_frameArena;