#pragma once
#include <EVA/Common.hpp>
#include <string.h>

#define STRING_PRINTF_ARGS(str) (int)(str).size, (const char*)(str).data

/**
 ** A non-owning string view.
 **/
struct String {
	U8*    data   = nullptr;
	size_t size   = 0;

	String() {};
	String(U8* data, size_t size) : data(data), size(size) {}
	String(const char* cstring) : data((U8*)cstring), size(strlen(cstring)) {}

	U8& operator[](size_t idx) { return data[idx]; }
	const U8& operator[](size_t idx) const { return data[idx]; }

	bool operator==(const String& other) const {
		return size == other.size && memcmp(data, other.data, size) == 0;
	}
	bool operator==(const char* other) const {
		size_t other_size = strlen(other);
		return size == other_size && memcmp(data, other, size) == 0;
	}

	String Skip(size_t n) {
		if (n > size) return {};
		return String(data + n, size - n);
	}
};

/**
 ** ZTString is a wrapper around String that's guarenteed to be zero-terminated.
 **/
struct ZTString {
	String string = {};

	ZTString() {}
	ZTString(const char* cstring) : string(cstring) {}
	explicit ZTString(String string) : string(string) {}

	operator String() { return string; }
	operator char*() { return (char*)string.data; }
	char* c_str() { return (char*)string.data; }

	bool operator==(const String& other) const { return string == other; }
	bool operator==(const char* other) const { return string == other; }
};