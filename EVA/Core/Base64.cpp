#include <EVA/Core/Common.hpp>

namespace Base64 {

static const char g_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t GetEncodedBufferSize(size_t inputSize) {
	return 4 * ((inputSize + 2) / 3);
}

static U8 DecodeChar(U8 ch) {
	if (ch >= 'A' && ch <= 'Z') return ch - 'A';
	if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
	if (ch >= '0' && ch <= '9') return ch - '0' + 52;
	if (ch == '+') return 62;
	if (ch == '/') return 63;
	return 0;
}

void Encode(U8* outBuffer, const void* _inBuffer, size_t inBufferSize) {
	U8* inBuffer = (U8*)_inBuffer;

	int in_idx = 0, out_idx = 0;
	while (in_idx + 3 <= inBufferSize) {
		U8 a = inBuffer[in_idx];
		U8 b = inBuffer[in_idx + 1];
		U8 c = inBuffer[in_idx + 2];
		U32 abc = (a << 16) | (b << 8) | c;

		outBuffer[out_idx + 0] = g_alphabet[(abc & 0b11111100'00000000'00000000) >> 18];
		outBuffer[out_idx + 1] = g_alphabet[(abc & 0b00000011'11110000'00000000) >> 12];
		outBuffer[out_idx + 2] = g_alphabet[(abc & 0b00000000'00001111'11000000) >> 6];
		outBuffer[out_idx + 3] = g_alphabet[(abc & 0b00000000'00000000'00111111) >> 0];

		in_idx += 3;
		out_idx += 4;
	}

	int rem = inBufferSize - in_idx;

	if (rem == 2) {
		U8 a = inBuffer[in_idx];
		U8 b = inBuffer[in_idx + 1];
		U32 ab = (a << 8) | b;

		outBuffer[out_idx + 0] = g_alphabet[(ab & 0b11111100'00000000) >> 10];
		outBuffer[out_idx + 1] = g_alphabet[(ab & 0b00000011'11110000) >> 4];
		outBuffer[out_idx + 2] = g_alphabet[(ab & 0b00000000'00001111)];
		outBuffer[out_idx + 3] = '=';
	} else if (rem == 1) {
		U8 a = inBuffer[in_idx];
		outBuffer[out_idx + 0] = g_alphabet[(a & 0b111100) >> 2];
		outBuffer[out_idx + 1] = g_alphabet[(a & 0b000011)];
		outBuffer[out_idx + 2] = '=';
		outBuffer[out_idx + 3] = '=';
	}
}

size_t Decode(void* _outBuffer, size_t outBufferSize, const U8* inBuffer, size_t inBufferSize) {
	assert(inBufferSize % 4 == 0);

	U8* outBuffer = (U8*)_outBuffer;

	size_t in_idx = 0;
	size_t out_idx = 0;

	while (in_idx < inBufferSize) {
		U32 out = 0;

		out |= DecodeChar(inBuffer[in_idx + 0]) << 18;
		out |= DecodeChar(inBuffer[in_idx + 1]) << 12;
		out |= DecodeChar(inBuffer[in_idx + 2]) << 6;
		out |= DecodeChar(inBuffer[in_idx + 3]) << 0;

		outBuffer[out_idx + 0] = (out & 0xFF0000) >> 16;
		if (out_idx + 1 < outBufferSize) outBuffer[out_idx + 1] = (out & 0x00FF00) >> 8;
		if (out_idx + 2 < outBufferSize) outBuffer[out_idx + 2] = (out & 0x0000FF) >> 0;

		in_idx += 4;
		out_idx += 3;
	}

	size_t outSize = (inBufferSize * 3) / 4;

	if (inBufferSize >= 4) {
		if (inBuffer[inBufferSize - 2] == '=') outSize -= 2;
		else if (inBuffer[inBufferSize - 1] == '=') outSize -= 1;
	}
	return outSize;
}

}