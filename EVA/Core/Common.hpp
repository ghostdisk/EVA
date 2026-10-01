#pragma once
#include <stdio.h>
#include <stdint.h>
#include <initializer_list>
#include <assert.h>
#include <string.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

template <typename T>
struct Slice
{
	T* data = nullptr;
	uint32 count = 0;

	Slice() = default;
	Slice(T* data, uint32 count) : data(data), count(count) {}
	Slice(std::initializer_list<T> values) : data((T*)values.begin()), count((uint32)values.size()) {}

	template <uint32 N>
	Slice(T (&values)[N]) : data(values), count(N) {}

	T& operator[](uint32 index) const { return data[index]; }
};

// Sized, non-owning view of a string. Not necessarily zero terminated.
struct StringView
{
	uint8* data = nullptr;
	size_t length = 0;

	StringView() = default;
	StringView(const char* cstring) : data((uint8*)cstring), length(cstring ? strlen(cstring) : 0) {}
	StringView(const char* string, size_t length) : data((uint8*)string), length(length) {}

	// True if non-empty.
	explicit operator bool() const
	{ 
		return length > 0;
	}
};

// A StringView that is also zero terminated, usable anywhere a StringView is. Never null: empty views point at "".
struct ZTStringView : StringView
{
	ZTStringView() : StringView("", 0) {}
	ZTStringView(const char* cstring) : StringView(cstring ? cstring : "") {}

	const char* CString() const
	{
		return (const char*)data;
	}
};

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
