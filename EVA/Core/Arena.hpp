#pragma once
#include <EVA/Core/Common.hpp>
#include <new>
#include <stdarg.h>

namespace EVA
{

// EVA_ARENA_BLOCK_SIZE (64K by default), rounded up to the large page size if large pages are available.
size_t GetArenaBlockSize();

struct ArenaBlock;

struct ArenaMark
{
	ArenaBlock* block = nullptr;
	uint8* head = nullptr;
};

// Bump allocator over blocks from a global thread-safe pool. Grows a block at a time; allocations larger than a block
// get their own. Allocations never fail, running out of memory panics.
// Block layout: | header | Arena (first block only) | data |
struct Arena
{
	uint8* head = nullptr;
	uint8* end = nullptr;          // of the current block
	ArenaBlock* current = nullptr; // newest block, chained to the older ones

	// Unaligned, so it's contiguous with the previous allocation if it fits in the current block.
	void* Allocate(size_t size);
	void* Allocate(size_t size, size_t alignment);

	template <typename T>
	T* New()
	{
		return new (Allocate(sizeof(T), alignof(T))) T();
	}

	ArenaMark Mark() const;
	void Rewind(ArenaMark mark);

	// For checks and debugging, walks the blocks.
	bool Contains(const void* pointer) const;
};

Arena* CreateArena();
void DestroyArena(Arena* arena);

// Debugging: blocks are filled with this byte when an arena takes them, -1 (default) for no fill.
void SetArenaFill(int fill);

// Copies the string into the arena, zero terminated.
ZTStringView InternString(Arena* arena, StringView string);

// printf into the arena. Returns an empty string if formatting fails.
ZTStringView avprintf(Arena* arena, const char* format, va_list args);
ZTStringView aprintf(Arena* arena, const char* format, ...);

}
