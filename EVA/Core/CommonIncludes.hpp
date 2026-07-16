#pragma once

#include <stdint.h>
#include <assert.h>
#include <sal.h>
#include <corecrt_malloc.h>
#include <tracy/Tracy.hpp>

#define COMPILE_SPEED_OPTIMIZATIONS 1

// This declarationslop is to allow us to use common operations without including some CRT headers, which on Windows
// increase compile times by quite a bit. This is non-standard as hell, if you hit errors, feel free to toggle this define
// until it's fixed.

#ifdef COMPILE_SPEED_OPTIMIZATIONS
extern "C" {
_NODISCARD _Check_return_ int __cdecl memcmp(_In_reads_bytes_(_Size) void const *_Buf1, _In_reads_bytes_(_Size) void const *_Buf2, _In_ size_t _Size);
void *__cdecl memset(_Out_writes_bytes_all_(_Size) void *_Dst, _In_ int _Val, _In_ size_t _Size);
void *__cdecl memcpy(_Out_writes_bytes_all_(_Size) void *_Dst, _In_reads_bytes_(_Size) void const *_Src, _In_ size_t _Size);
_Check_return_ size_t __cdecl strlen(_In_z_ char const *_Str);
}

template <typename T>
constexpr void Swap(T& a, T& b) noexcept {
    T tmp = static_cast<T&&>(a);
    a = static_cast<T&&>(b);
    b = static_cast<T&&>(tmp);
}

typedef struct _iobuf FILE; 
#else
#	include <stdio.h>
#	include <stdlib.h>
#	include <string.h>
#endif