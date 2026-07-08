#pragma once
#include <EVA/Core/String.hpp>

struct Arena;

#define TRY(expr) \
	do { \
		Result res = (expr); \
		if (!res) return res; \
	} while (0)

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