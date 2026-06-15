#pragma once

struct float2
{
	float x = 0.0f;
	float y = 0.0f;

	float2() {}
	float2(float _x, float _y) : x(_x), y(_y) {}
};

struct float3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float3() {}
	float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct float4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	float4() {}
	float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};
