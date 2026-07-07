#pragma once
#include <EVA/Common.hpp>
#include <string.h>

#define STRING_PRINTF_ARGS(str) (int)(str).size, (const char*)(str).data

struct String;
struct ZTString;

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

	String Skip(size_t n) const {
		if (n >= size) return {};
		return String(data + n, size - n);
	}

	String Take(size_t n) const {
		if (n >= size) return String(data, size);
		return String(data, n);
	}

	inline ZTString CopyToHeap() const;
};

/**
 ** ZTString is a wrapper around String that's guarenteed to be zero-terminated.
 **/
struct ZTString : String {
	ZTString() {}
	ZTString(const char* cstring) : String(cstring) {}
	explicit ZTString(U8* data, size_t size) : String(data, size) {}
	explicit ZTString(String string) : String(string) {}

	operator char*() { return (char*)data; }
	operator const char*() const { return (const char*)data; }
	char* c_str() { return (char*)data; }
	const char* c_str() const { return (char*)data; }
};

ZTString String::CopyToHeap() const {
	U8* copy_buffer = (U8*)malloc(size + 1);
	memcpy(copy_buffer, data, size);
	copy_buffer[size] = '\0';
	return ZTString(copy_buffer, size);
}