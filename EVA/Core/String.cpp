#include <EVA/Core/Basic.hpp>
#include <stdarg.h>

void StringBuilder::Push(String str) {
	size_t start = buffer.size();
	buffer.resize(buffer.size() + str.size);
	memcpy(buffer.data() + start, str.data, str.size);
}

void StringBuilder::Push(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String s = Vfmt(FrameArena, fmt, args);
	Push(s);
	va_end(args);
}

ZTString StringBuilder::Build() {
	buffer.push_back('\0');
	return ZTString(buffer.data(), buffer.size() - 1);
}

ZTString String::CopyToArena(Arena* arena) const {
	U8* copy = (U8*)ArenaAllocate(arena, size + 1);
	memcpy(copy, data, size);
	copy[size] = '\0';
	return ZTString(copy, size);
}

ZTString String::CopyToHeap() const {
	U8* copy_buffer = (U8*)malloc(size + 1);
	memcpy(copy_buffer, data, size);
	copy_buffer[size] = '\0';
	return ZTString(copy_buffer, size);
}