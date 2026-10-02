#include <EVA/Core/Arena.hpp>
#include <EVA/Core/Panic.hpp>
#include <EVA/Core/VirtualMemory.hpp>
#include <atomic>

// Under ASan, memory outside of allocations is poisoned. EVA_ARENA_REDZONE (fuzzing builds) also leaves poisoned bytes
// before each aligned allocation.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define EVA_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(EVA_ASAN)
#define EVA_ASAN 1
#endif

#ifdef EVA_ASAN
#include <sanitizer/asan_interface.h>
#define POISON(memory, size) ASAN_POISON_MEMORY_REGION(memory, size)
#define UNPOISON(memory, size) ASAN_UNPOISON_MEMORY_REGION(memory, size)
#else
#define POISON(memory, size) ((void)(memory), (void)(size))
#define UNPOISON(memory, size) ((void)(memory), (void)(size))
#endif

#ifndef EVA_ARENA_REDZONE
#define EVA_ARENA_REDZONE 0
#endif

#ifndef EVA_ARENA_BLOCK_SIZE
#define EVA_ARENA_BLOCK_SIZE (64 * 1024)
#endif

namespace EVA
{

struct ArenaBlock
{
	// Next free block in the pool, previous block in an arena. Atomic since a losing PopBlock can read it while the
	// winner reuses the block.
	std::atomic<ArenaBlock*> next = nullptr;
	size_t size = 0; // header included. Larger than the block size for oversized allocations
};

static size_t RoundUp(size_t size, size_t alignment)
{
	return (size + alignment - 1) / alignment * alignment;
}

static uint8* BlockData(ArenaBlock* block)
{
	return (uint8*)block + sizeof(ArenaBlock);
}

static uint8* BlockEnd(ArenaBlock* block)
{
	return (uint8*)block + block->size;
}

size_t GetArenaBlockSize()
{
	static size_t size = []
	{
		size_t configured = RoundUp(EVA_ARENA_BLOCK_SIZE, VIRTUAL_MEMORY_ALIGNMENT);
		size_t large_page_size = GetLargePageSize();
		return large_page_size ? RoundUp(configured, large_page_size) : configured;
	}();
	return size;
}

static std::atomic<int> arena_fill = -1;

void SetArenaFill(int fill)
{
	arena_fill.store(fill, std::memory_order_relaxed);
}

static void FillBlock(ArenaBlock* block)
{
	int fill = arena_fill.load(std::memory_order_relaxed);
	if (fill < 0)
		return;
	uint8* data = BlockData(block);
	size_t size = block->size - sizeof(ArenaBlock);
	UNPOISON(data, size);
	memset(data, fill, size);
	POISON(data, size);
}

// Lock-free stack of free blocks. Blocks are aligned, so the top's low bits hold a counter bumped on every change,
// against ABA. Pooled blocks are never returned to the OS, so reading a stale block's next is safe.
static std::atomic<uintptr_t> pool_top = 0;
static const uintptr_t COUNT_MASK = VIRTUAL_MEMORY_ALIGNMENT - 1;

static ArenaBlock* PopBlock()
{
	uintptr_t top = pool_top.load(std::memory_order_acquire);
	for (;;)
	{
		ArenaBlock* block = (ArenaBlock*)(top & ~COUNT_MASK);
		if (!block)
			return nullptr;
		ArenaBlock* next = block->next.load(std::memory_order_relaxed);
		uintptr_t new_top = (uintptr_t)next | ((top + 1) & COUNT_MASK);
		if (pool_top.compare_exchange_weak(top, new_top, std::memory_order_acquire, std::memory_order_acquire))
			return block;
	}
}

// first to last, already chained through next.
static void PushBlocks(ArenaBlock* first, ArenaBlock* last)
{
	uintptr_t top = pool_top.load(std::memory_order_relaxed);
	for (;;)
	{
		last->next.store((ArenaBlock*)(top & ~COUNT_MASK), std::memory_order_relaxed);
		uintptr_t new_top = (uintptr_t)first | ((top + 1) & COUNT_MASK);
		if (pool_top.compare_exchange_weak(top, new_top, std::memory_order_release, std::memory_order_relaxed))
			return;
	}
}

static ArenaBlock* NewBlock(size_t size, bool large_pages)
{
	ArenaBlock* block = (ArenaBlock*)AllocateVirtualMemory(size, large_pages);
	if (!block)
		return nullptr;
	new (block) ArenaBlock();
	block->size = RoundUp(size, VIRTUAL_MEMORY_ALIGNMENT);
	POISON(BlockData(block), block->size - sizeof(ArenaBlock));
	return block;
}

static ArenaBlock* AcquireBlock()
{
	ArenaBlock* block = PopBlock();
	if (!block)
	{
		size_t size = GetArenaBlockSize();
		if (GetLargePageSize())
			block = NewBlock(size, true);
		if (!block) // large pages can run out once physical memory is fragmented
			block = NewBlock(size, false);
		if (!block)
			Panic("out of memory: failed to allocate a %zu byte arena block", size);
	}
	FillBlock(block);
	return block;
}

// From block down to, not including, stop. Oversized blocks go back to the OS.
static void ReleaseBlocks(ArenaBlock* block, ArenaBlock* stop)
{
	size_t block_size = GetArenaBlockSize();
	ArenaBlock* first = nullptr;
	ArenaBlock* last = nullptr;
	while (block != stop)
	{
		ArenaBlock* next = block->next.load(std::memory_order_relaxed);
		if (block->size != block_size)
			FreeVirtualMemory(block, block->size);
		else
		{
			POISON(BlockData(block), block->size - sizeof(ArenaBlock));
			block->next.store(first, std::memory_order_relaxed);
			first = block;
			if (!last)
				last = block;
		}
		block = next;
	}
	if (first)
		PushBlocks(first, last);
}

static void Grow(Arena& arena, size_t size, size_t alignment, size_t redzone)
{
	if (size > SIZE_MAX / 4 || alignment > SIZE_MAX / 4)
		Panic("arena allocation of %zu bytes is too large", size);

	ArenaBlock* block = nullptr;
	size_t needed = sizeof(ArenaBlock) + redzone + alignment - 1 + size;
	if (needed <= GetArenaBlockSize())
		block = AcquireBlock();
	else
	{
		block = NewBlock(needed, false);
		if (!block)
			Panic("out of memory: failed to allocate %zu bytes for an arena", size);
		FillBlock(block);
	}

	block->next.store(arena.current, std::memory_order_relaxed);
	arena.current = block;
	arena.head = BlockData(block);
	arena.end = BlockEnd(block);
}

// Can be past the block's end.
static uintptr_t AllocationStart(Arena& arena, size_t alignment, size_t redzone)
{
	return ((uintptr_t)arena.head + redzone + alignment - 1) & ~(uintptr_t)(alignment - 1);
}

static void* Bump(Arena& arena, size_t size, size_t alignment, size_t redzone)
{
	assert(alignment && (alignment & (alignment - 1)) == 0);
	uintptr_t start = AllocationStart(arena, alignment, redzone);
	if (start > (uintptr_t)arena.end || size > (uintptr_t)arena.end - start)
	{
		Grow(arena, size, alignment, redzone);
		start = AllocationStart(arena, alignment, redzone);
	}

	uint8* memory = (uint8*)start;
	arena.head = memory + size;
	UNPOISON(memory, size);
	return memory;
}

void* Arena::Allocate(size_t size)
{
	return Bump(*this, size, 1, 0);
}

void* Arena::Allocate(size_t size, size_t alignment)
{
	return Bump(*this, size, alignment, EVA_ARENA_REDZONE);
}

ArenaMark Arena::Mark() const
{
	return { current, head };
}

void Arena::Rewind(ArenaMark mark)
{
	ReleaseBlocks(current, mark.block);
	current = mark.block;
	head = mark.head;
	end = BlockEnd(current);
	POISON(head, end - head);
}

bool Arena::Contains(const void* pointer) const
{
	for (ArenaBlock* block = current; block; block = block->next.load(std::memory_order_relaxed))
	{
		uint8* used_end = block == current ? head : BlockEnd(block);
		if ((const uint8*)pointer >= BlockData(block) && (const uint8*)pointer < used_end)
			return true;
	}
	return false;
}

Arena* CreateArena()
{
	ArenaBlock* block = AcquireBlock();
	block->next.store(nullptr, std::memory_order_relaxed);
	uint8* data = BlockData(block);
	UNPOISON(data, sizeof(Arena));
	Arena* arena = new (data) Arena();
	arena->current = block;
	arena->head = data + sizeof(Arena);
	arena->end = BlockEnd(block);
	return arena;
}

void DestroyArena(Arena* arena)
{
	ReleaseBlocks(arena->current, nullptr);
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
