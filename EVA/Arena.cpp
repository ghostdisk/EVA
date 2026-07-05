#include <EVA/Arena.hpp>
#include <EVA/String.hpp>
#include <EVA/Result.hpp>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define ARENA_SIZE (1 << 20u) // 1MB

Arena* FrameArenas[NUM_FRAME_ARENAS] = {};
Arena* FrameArena = nullptr;

void ArenaInitialize() {
	for (int i = 0; i < NUM_FRAME_ARENAS; i++) {
		FrameArenas[i] = ArenaCreate();
	}
}

void RotateFrameArenas() {
	static int k = 0;
	k++;
	if (k >= NUM_FRAME_ARENAS) k = 0;

	FrameArena = FrameArenas[k];
	ArenaReset(FrameArena);
}

Arena* ArenaCreate() {
	Arena* arena = new Arena();
	arena->begin = (U8*)malloc(ARENA_SIZE);
	arena->head = arena->begin;
	arena->end = arena->begin + ARENA_SIZE;
	return arena;
}

void ArenaDestroy(Arena* arena) {
	free(arena->begin);
	delete arena;
}

void* ArenaAllocate(Arena* arena, size_t size) {
	U8* ptr = arena->head;
	if (size > (arena->end - arena->head)) {
		Fatal("ArenaAllocate: out of memory");
	}
	arena->head += size;
	return ptr;

}

void* ArenaAllocate(Arena* arena, size_t size, size_t alignment) {
	ArenaAlignHead(arena, alignment);
	return ArenaAllocate(arena, size);
}

void ArenaAlignHead(Arena* arena, size_t alignment) {
	uintptr_t head = (uintptr_t)arena->head;
	uintptr_t mask = (uintptr_t)alignment - 1;
	head = (head + mask) & (~mask);
	arena->head = (U8*)head;
}

char* ArenaInternCString(Arena* arena, const char* cstring, int len) {
	if (len < 0) len = strlen(cstring);

	char* copy = (char*)ArenaAllocate(arena, len + 1);
	memcpy(copy, cstring, len);
	copy[len] = '\0';
	return copy;
}

ZTString Vfmt(Arena* arena, const char* fmt, va_list args) {
	va_list measure_args;
	va_copy(measure_args, args);
	char dummy_buffer[1];
	int length = vsnprintf(dummy_buffer, 0, fmt, measure_args);
	va_end(measure_args);

	if (length < 0) {
		return {};
	}

	U8* buffer = (U8*)ArenaAllocate(arena, length + 1);
	if (buffer) {
		va_list write_args;
		va_copy(write_args, args);
		vsnprintf((char*)buffer, length + 1, fmt, write_args);
		va_end(write_args);
	}

	return ZTString(String(buffer, length));
}

ZTString Fmt(Arena* arena, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ZTString result = Vfmt(arena, fmt, args);
	va_end(args);
	return result;
}

void ArenaReset(Arena* arena) {
	arena->head = arena->begin;
}

Result Err(const char* fmt, ...) {
	Arena* arena = FrameArena;

	va_list args;
	va_start(args, fmt);

	ZTString* error_string = (ZTString*)ArenaAllocate(arena, sizeof(ZTString), alignof(ZTString));
	*error_string = Vfmt(arena, fmt, args);
	return Result{ .error = error_string };
}