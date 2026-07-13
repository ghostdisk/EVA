#pragma once
#include <EVA/Core/Common.hpp>

struct AllocatorVTable;

struct Allocator {
	using AllocateProc  = void* (*)(void* userdata, size_t size, size_t alignment);
	using FreeProc      = void  (*)(void* userdata, void* data);

	void*            userdata = nullptr;
	AllocatorVTable* vtable   = nullptr;

	inline void* Allocate(size_t size, size_t alignment);
	inline void  Free(void* ptr);
	inline void  FreeSized(void* ptr, size_t size);

	static Allocator HeapAllocator;
};

struct AllocatorVTable {
	Allocator::AllocateProc  allocate     = nullptr;
	Allocator::FreeProc      free         = nullptr;
};

void* Allocator::Allocate(size_t size, size_t alignment) {
	return vtable->allocate(userdata, size, alignment);
}

void Allocator::Free(void* ptr) {
	return vtable->free(userdata, ptr);
}