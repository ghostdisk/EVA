#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA
{

typedef void (*PanicHandler)(ZTStringView message);

// Unrecoverable error. Formats the message and passes it to the panic handler. By default that prints it and exits.
[[noreturn]] void Panic(const char* format, ...);

// Replaces the default print and exit, e.g. the test runner throws so a panic fails just the current test.
// The message is only valid during the call. If the handler returns, Panic still exits. nullptr restores the default.
void SetPanicHandler(PanicHandler handler);

}
