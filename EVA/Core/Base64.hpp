#pragma once
#include <EVA/Core/Basic.hpp>

namespace Base64 {

size_t GetEncodedBufferSize(size_t bytesSize);

void Encode(U8* outBuffer, const void* _inBuffer, size_t inBufferSize);
size_t Decode(void* _outBuffer, size_t outBufferSize, const U8* inBuffer, size_t inBufferSize);

};