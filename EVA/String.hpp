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
};