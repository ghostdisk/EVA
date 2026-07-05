#pragma once
#include <EVA/String.hpp>

struct Arena;

struct Result {
	// both error's ptr and the data itself are arena allocated.
	// error is arena-allocated so the result fits in 1 register.
	ZTString* error = nullptr;

	operator bool() { return error == nullptr; }
};

inline Result Success() {
	return Result();
}

Result Err(const char* fmt, ...);