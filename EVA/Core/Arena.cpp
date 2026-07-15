#include <EVA/Core/Basic.hpp>
#include <stdio.h>
#include <stdarg.h>

#define ARENA_SIZE (1 << 20u) // 1MB

Arena* g_frameArenas[NUM_FRAME_ARENAS] = {};
Arena* g_frameArena = nullptr;
static Arena* g_scratchArena = nullptr;

void ArenaInitialize() {
	g_scratchArena = Arena::Create();

	for (int i = 0; i < NUM_FRAME_ARENAS; i++) {
		g_frameArenas[i] = Arena::Create();
	}
}

void Arena::RotateFrameArenas() {
	static int k = 0;
	k++;
	if (k >= NUM_FRAME_ARENAS) k = 0;

	g_frameArena = g_frameArenas[k];
	g_frameArena->Reset();
}

Arena* Arena::Create() {
	Arena* arena = new Arena();
	arena->m_begin = (U8*)malloc(ARENA_SIZE);
	arena->m_head = arena->m_begin;
	arena->m_end = arena->m_begin + ARENA_SIZE;
	return arena;
}

void Arena::Destroy() {
	free(m_begin);
	delete this;
}

void* Arena::Allocate(size_t size) {
	U8* ptr = m_head;
	if (size > (m_end - m_head)) {
		Fatal("Arena::Allocate: out of memory");
	}
	m_head += size;
	return ptr;

}

void* Arena::Allocate(size_t size, size_t alignment) {
	AlignHead(alignment);
	return Allocate(size);
}

void Arena::AlignHead(size_t alignment) {
	uintptr_t head = (uintptr_t)m_head;
	uintptr_t mask = (uintptr_t)alignment - 1;
	head = (head + mask) & (~mask);
	m_head = (U8*)head;
}

ZTString Arena::Vfmt(const char* fmt, va_list args) {
	va_list measure_args;
	va_copy(measure_args, args);
	char dummy_buffer[1];
	int length = vsnprintf(dummy_buffer, 0, fmt, measure_args);
	va_end(measure_args);

	if (length < 0) {
		return {};
	}

	U8* buffer = (U8*)Allocate(length + 1);
	if (buffer) {
		va_list write_args;
		va_copy(write_args, args);
		vsnprintf((char*)buffer, length + 1, fmt, write_args);
		va_end(write_args);
	}

	return ZTString(String(buffer, length));
}

ZTString Arena::Fmt(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ZTString result = Vfmt(fmt, args);
	va_end(args);
	return result;
}

void Arena::Reset() {
	m_head = m_begin;
}

ArenaResetPoint Arena::GetResetPoint() {
	return ArenaResetPoint{
		.head = m_head,
	};
}

void Arena::Reset(ArenaResetPoint resetPoint) {
	m_head = resetPoint.head;
}

ScratchArena::ScratchArena() {
	m_arena = g_scratchArena;
	m_resetPoint = m_arena->GetResetPoint();
}

ScratchArena::~ScratchArena() {
	m_arena->Reset(m_resetPoint);
}

ScratchArena::operator Arena*() {
	return m_arena;
}

Arena* ScratchArena::operator->() {
	return m_arena;
}

Arena* ScratchArena::operator*() {
	return m_arena;
}
