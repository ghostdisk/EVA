#include <EVA/Core/Allocator.hpp>
#include <EVA/Core/Arena.hpp>
#include <stdlib.h>

static AllocatorVTable g_heap_allocator_vtable = {
	.allocate = [](void* userdata, size_t size, size_t alignment) -> void* {
		return _aligned_malloc(size, alignment);
	},
	.free = [](void* userdata, void* ptr) {
		// TODO: This does not work, msvc is crying that we should use aligned free.
		//       It also does not work for arenas, which is just lovely, so this allocator abstraction
		//       fails on every single possible level.
		free(ptr);
	},
};

static AllocatorVTable g_arena_allocator_vtable = {
	.allocate = [](void* arena, size_t size, size_t alignment) -> void* {
		return ((Arena*)arena)->Allocate(size, alignment);
	},
};

Allocator::Allocator(void* userdata, AllocatorVTable* vtable) : userdata(userdata), vtable(vtable) {}

Allocator::Allocator(Arena* arena) : Allocator(arena, &g_arena_allocator_vtable) {}

Allocator Allocator::HeapAllocator(nullptr, &g_heap_allocator_vtable);
