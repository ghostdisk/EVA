#pragma once
#include <EVA/Common.hpp>
#include <string.h>
#include <cglm/types.h>
#include <math.h>

#define COLOR_RGB(r, g, b) { float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1 }

#define DEG_TO_RAD (GLM_PIf / 180.0f)
#define RAD_TO_DEG (180.0f / GLM_PI)

struct float2
{
	float x = 0.0f;
	float y = 0.0f;

	float2() {}
	float2(float _x, float _y) : x(_x), y(_y) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently

	inline float SquaredLength() { return x*x + y*y; }
	inline float Length() { return sqrtf(x*x + y*y); }
};

inline float2 operator+(const float2& a, const float2& b) { return float2(a.x+b.x, a.y+b.y); }
inline float2 operator-(const float2& a, const float2& b) { return float2(a.x-b.x, a.y-b.y); }
inline float2 operator*(const float2& vec, float s) { return float2(vec.x*s, vec.y*s); }
inline float2 operator/(const float2& vec, float s) { return float2(vec.x/s, vec.y/s); }

inline void operator+=(float2& a, const float2& b) { a.x += b.x; a.y += b.y;  }
inline void operator-=(float2& a, const float2& b) { a.x -= b.x; a.y -= b.y;  }
inline void operator*=(float2& vec, float s) { vec.x *= s; vec.y *= s;  }
inline void operator/=(float2& vec, float s) { vec.x /= s; vec.y /= s;  }

inline float2 operator-(const float2& vec) { return float2(-vec.x, -vec.y); }

struct float3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float3() {}
	float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	float3(float* vec) : x(vec[0]), y(vec[1]), z(vec[2]) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently

	inline float SquaredLength() { return x*x + y*y + z*z; }
	inline float Length() { return sqrtf(x*x + y*y + z*z); }

	inline float3 Normalized();
};

inline float3 operator+(const float3& a, const float3& b) { return float3(a.x+b.x, a.y+b.y, a.z+b.z); }
inline float3 operator-(const float3& a, const float3& b) { return float3(a.x-b.x, a.y-b.y, a.z-b.z); }
inline float3 operator*(const float3& vec, float s) { return float3(vec.x*s, vec.y*s, vec.z*s); }
inline float3 operator/(const float3& vec, float s) { return float3(vec.x/s, vec.y/s, vec.z/s); }

inline void operator+=(float3& a, const float3& b) { a.x += b.x; a.y += b.y; a.z += b.z; }
inline void operator-=(float3& a, const float3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; }
inline void operator*=(float3& vec, float s) { vec.x *= s; vec.y *= s; vec.z *= s; }
inline void operator/=(float3& vec, float s) { vec.x /= s; vec.y /= s; vec.z /= s; }

inline float3 operator-(const float3& vec) { return float3(-vec.x, -vec.y, -vec.z); }

struct float4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	float4() {}
	float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
	float4(float* vec) : x(vec[0]), y(vec[1]), z(vec[2]), w(vec[3]) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently
};

struct __declspec(align(16)) float4x4
{
	float data[4][4]; // column major, indexed as [col][row]
	inline operator vec4*() { return &data[0]; } // needed so we can pss it to cglm conveniently

	float4& column(int c)
	{
		return *(float4*)&data[c];
	}

	static constexpr float4x4 Zero()
	{
		float4x4 mat;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				mat.data[i][j] = 0.0f;
			}
		}
		return mat;
	}

	static constexpr float4x4 Identity()
	{
		float4x4 mat = float4x4::Zero();
		mat.data[0][0] = 1.0f;
		mat.data[1][1] = 1.0f;
		mat.data[2][2] = 1.0f;
		mat.data[3][3] = 1.0f;
		return mat;
	}
};

struct Plane
{
	float3 normal   = {};
	float  distance = 0;

	Plane Invert()
	{
		return Plane(-normal, -distance);
	}
};


inline float3 float3::Normalized()
{
	float len = Length();
	if (len < 0.00001f)
	{
		return float3(0, 0, 0);
	}
	else
	{
		return (*this) / len;
	}
}


inline float4 operator-(const float4& vec) { return float4(-vec.x, -vec.y, -vec.z, -vec.w); }

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

inline float Dot(const float2& a, const float2& b) { return a.x*b.x + a.y*b.y; }
inline float Dot(const float3& a, const float3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float Dot(const float4& a, const float4& b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

inline float Distance(const float3& a, const float3& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return sqrt(dx*dx + dy*dy + dz*dz);
}

inline float3 Cross(const float3& a, const float3 b)
{
	return float3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}