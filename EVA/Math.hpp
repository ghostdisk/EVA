#pragma once
#include <string.h>

struct float2
{
	float x = 0.0f;
	float y = 0.0f;

	float2() {}
	float2(float _x, float _y) : x(_x), y(_y) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently
};

struct float3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float3() {}
	float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently
};

struct float4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	float4() {}
	float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently
};


typedef float __declspec(align(16)) float4_arr[4];

struct __declspec(align(16)) float4x4
{
	float data[4][4]; // column major, indexed as [col][row]
	inline operator float4_arr*() { return &data[0]; } // needed so we can pss it to cglm conveniently
};

inline void zero(float4x4& mat)
{
	memset(mat.data, 0, sizeof(mat));
}

inline void identity(float4x4& mat)
{
	memset(mat.data, 0, sizeof(mat));
	mat.data[0][0] = 1.0f;
	mat.data[1][1] = 1.0f;
	mat.data[2][2] = 1.0f;
	mat.data[3][3] = 1.0f;
}