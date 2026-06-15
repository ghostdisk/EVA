#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#define EVA_BASE_DIR "D:/EVA"

struct SDL_Window;

////////////////////////////////////////////////////////////////////////////////
// BASIC TYPES

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t   I8;
typedef int16_t  I16;
typedef int32_t  I32;
typedef int64_t  I64;
typedef float    F32;
typedef double   F64;

////////////////////////////////////////////////////////////////////////////////
// DEFER

template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define DEFER(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})


////////////////////////////////////////////////////////////////////////////////
// COMMON UTILS

[[noreturn]]
void Fatal(const char* fmt, ...);

bool ReadEntireFile(const char* path, void** out_data, size_t* out_size);

////////////////////////////////////////////////////////////////////////////////

extern SDL_Window* GameWindow;