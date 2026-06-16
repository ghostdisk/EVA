#pragma once
#include <EVA/Common.hpp>


U32 HashU32(U32 value, U32 seed = 0);
U32 HashI32(I32 value, U32 seed = 0);
U32 HashBytes(const void * key, int len, U32 seed = 0);