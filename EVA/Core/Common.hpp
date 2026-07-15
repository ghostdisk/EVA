#pragma once

#include <EVA/Core/CommonIncludes.hpp>

#define EVA_BASE_DIR "D:/EVA"

#define EVA_ARRAYSIZE(arr) (sizeof((arr))/sizeof((arr)[0]))

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

[[noreturn]] void Fatal(const char* fmt, ...);
bool ReadEntireFile(const char* path, void** out_data, size_t* out_size);
void ScreenLog(const char* fmt, ...);
void QueueForNextFrame(void (*callback)(void* userdata), void* userdata);

/////////////////////////////////////////////////////////////////////////////////

//namespace std {
template<class T> struct remove_reference      { using type = T; };
template<class T> struct remove_reference<T&>  { using type = T; };
template<class T> struct remove_reference<T&&> { using type = T; };

template<class T>
constexpr typename remove_reference<T>::type&& Move(T&& value) noexcept {
    return static_cast<typename remove_reference<T>::type&&>(value);
}
