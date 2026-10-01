#include <EVA/Core/Panic.hpp>
#include <stdarg.h>
#include <stdlib.h>

namespace EVA
{

static PanicHandler panic_handler = nullptr;

void Panic(const char* format, ...)
{
	// Formatted on the stack, since panics can come from the allocators.
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	if (panic_handler)
		panic_handler(message);

	fprintf(stderr, "panic: %s\n", message);
	exit(1);
}

void SetPanicHandler(PanicHandler handler)
{
	panic_handler = handler;
}

}
