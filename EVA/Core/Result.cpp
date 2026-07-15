#include <EVA/Core/Basic.hpp>
#include <stdarg.h>

Result Err(const char* fmt, ...) {
	Arena* arena = g_frameArena;

	va_list args;
	va_start(args, fmt);

	ZTString* error_string = (ZTString*)arena->Allocate(sizeof(ZTString), alignof(ZTString));
	*error_string = arena->Vfmt(fmt, args);
	return Result{ .error = error_string };
}
