#include <EVA/Core/Basic.hpp>

Result Err(const char* fmt, ...) {
	Arena* arena = FrameArena;

	va_list args;
	va_start(args, fmt);

	ZTString* error_string = (ZTString*)ArenaAllocate(arena, sizeof(ZTString), alignof(ZTString));
	*error_string = Vfmt(arena, fmt, args);
	return Result{ .error = error_string };
}