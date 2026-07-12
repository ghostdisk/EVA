#include <EVA/Core/Allocator.hpp>
#include <stdlib.h>

static AllocatorVTable g_heap_allocator_vtable = {
	.allocate = [](void* userdata, size_t size, size_t alignment) -> void* {
		return _aligned_malloc(size, alignment);
	},
	.free = [](void* userdata, void* ptr) {
		free(ptr);
	},
};

Allocator Allocator::HeapAllocator = {
	.userdata = nullptr,
	.vtable = &g_heap_allocator_vtable,
};