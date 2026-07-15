#pragma once
#include <EVA/Core/Basic.hpp>
#include <cglm/types.h>
#include <math.h>
#include <string.h>

class Serializer;
class Deserializer;

#define COLOR_RGB(r, g, b) { float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1 }
#define COLOR_RGBA(r, g, b, a) { float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f }
#define COLOR_WHITE (float4{1,1,1,1})
#define COLOR_BLACK (float4{0,0,0,1})

#define DEG_TO_RAD (GLM_PIf / 180.0f)
#define RAD_TO_DEG (180.0f / GLM_PI)

#define XY(v) v.x, v.y
#define XYZ(v) v.x, v.y, v.z

struct float2 {
	float x = 0.0f;
	float y = 0.0f;

	float2() {}
	float2(float _x, float _y) : x(_x), y(_y) {}
	inline operator float*() { return &x; } // needed so we can pass it to cglm conveniently

	inline float SquaredLength() { return x*x + y*y; }
	inline float Length() { return sqrtf(x*x + y*y); }
};

void Serialize(Serializer& s, const float2& f);
void Deserialize(Deserializer& d, float2& f);

inline float2 operator+(const float2& a, const float2& b) { return float2(a.x+b.x, a.y+b.y); }
inline float2 operator-(const float2& a, const float2& b) { return float2(a.x-b.x, a.y-b.y); }
inline float2 operator*(const float2& vec, float s) { return float2(vec.x*s, vec.y*s); }
inline float2 operator*(float s, const float2& vec) { return float2(vec.x*s, vec.y*s); }
inline float2 operator/(const float2& vec, float s) { return float2(vec.x/s, vec.y/s); }

inline void operator+=(float2& a, const float2& b) { a.x += b.x; a.y += b.y;  }
inline void operator-=(float2& a, const float2& b) { a.x -= b.x; a.y -= b.y;  }
inline void operator*=(float2& vec, float s) { vec.x *= s; vec.y *= s;  }
inline void operator/=(float2& vec, float s) { vec.x /= s; vec.y /= s;  }

inline float2 operator-(const float2& vec) { return float2(-vec.x, -vec.y); }

struct float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float3() {}
	float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	float3(float* vec) : x(vec[0]), y(vec[1]), z(vec[2]) {}
	inline operator float*() const { return (float*)&x; } // needed so we can pass it to cglm conveniently

	inline float SquaredLength() { return x*x + y*y + z*z; }
	inline float Length() { return sqrtf(x*x + y*y + z*z); }

	inline float3 Normalized();

	inline       float2& xy()       { return *(float2*)this; }
	inline const float2& xy() const { return *(float2*)this; }
};

void Serialize(Serializer& s, const float3& f);
void Deserialize(Deserializer& d, float3& f);

inline float3 operator+(const float3& a, const float3& b) { return float3(a.x+b.x, a.y+b.y, a.z+b.z); }
inline float3 operator-(const float3& a, const float3& b) { return float3(a.x-b.x, a.y-b.y, a.z-b.z); }
inline float3 operator*(const float3& vec, float s) { return float3(vec.x*s, vec.y*s, vec.z*s); }
inline float3 operator*(float s, const float3& vec) { return float3(vec.x*s, vec.y*s, vec.z*s); }
inline float3 operator/(const float3& vec, float s) { return float3(vec.x/s, vec.y/s, vec.z/s); }

inline void operator+=(float3& a, const float3& b) { a.x += b.x; a.y += b.y; a.z += b.z; }
inline void operator-=(float3& a, const float3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; }
inline void operator*=(float3& vec, float s) { vec.x *= s; vec.y *= s; vec.z *= s; }
inline void operator/=(float3& vec, float s) { vec.x /= s; vec.y /= s; vec.z /= s; }

inline float3 operator-(const float3& vec) { return float3(-vec.x, -vec.y, -vec.z); }

struct __declspec(align(16)) float4 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	float4() {}
	float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
	float4(const float3& xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	float4(float* vec) : x(vec[0]), y(vec[1]), z(vec[2]), w(vec[3]) {}
	inline operator float*() const { return (float*)&x; } // needed so we can pass it to cglm conveniently

	inline       float3& xyz()       { return *(float3*)this; }
	inline const float3& xyz() const { return *(float3*)this; }
};

void Serialize(Serializer& s, const float4& f);
void Deserialize(Deserializer& d, float4& f);

struct __declspec(align(16)) float4x4 {
	float data[4][4]; // column major, indexed as [col][row]
	inline operator vec4*() const { return (vec4*)&data[0]; } // needed so we can pss it to cglm conveniently

	float4& column(int c) {
		return *(float4*)&data[c];
	}

	static constexpr float4x4 Zero() {
		float4x4 mat;
		for (int i = 0; i < 4; i++) 
			for (int j = 0; j < 4; j++) 
				mat.data[i][j] = 0.0f;
		return mat;
	}

	static constexpr float4x4 Identity() {
		float4x4 mat = float4x4::Zero();
		mat.data[0][0] = 1.0f;
		mat.data[1][1] = 1.0f;
		mat.data[2][2] = 1.0f;
		mat.data[3][3] = 1.0f;
		return mat;
	}

	static float4x4 FromTransform(const float3& position);
	static float4x4 FromTransform(const float3& position, const float4& rotation);
	static float4x4 FromTransform(const float3& position, const float4& rotation, const float3& scale);

	float3 TransformPosition(float3 pos) const;
};

float4 operator*(const float4x4& mat, const float4& p);
float4x4 operator*(const float4x4& mat, const float4x4& p);

inline float3 float3::Normalized() {
	float len = Length();
	if (len < 0.00001f) {
		return float3(0, 0, 0);
	} else {
		return (*this) / len;
	}
}


inline float4 operator-(const float4& vec) { return float4(-vec.x, -vec.y, -vec.z, -vec.w); }

inline void zero(float4x4& mat) {
	memset(mat.data, 0, sizeof(mat));
}

inline void identity(float4x4& mat) {
	memset(mat.data, 0, sizeof(mat));
	mat.data[0][0] = 1.0f;
	mat.data[1][1] = 1.0f;
	mat.data[2][2] = 1.0f;
	mat.data[3][3] = 1.0f;
}

inline float Dot(const float2& a, const float2& b) { return a.x*b.x + a.y*b.y; }
inline float Dot(const float3& a, const float3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float Dot(const float4& a, const float4& b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

float Unlerp(float2 a, float2 mid, float2 b);

inline float Distance(const float2& a, const float2& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return sqrt(dx*dx + dy*dy);
}

inline float Distance(const float3& a, const float3& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return sqrt(dx*dx + dy*dy + dz*dz);
}

inline float3 Cross(const float3& a, const float3 b) {
	return float3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

struct Plane {
	float3 normal   = {};
	float  distance = 0;

	Plane Invert() {
		return Plane(-normal, -distance);
	}
};

struct AABB {
	float3 min;
	float3 max;

	void Init(float3 p) {
		min = p;
		max = p;
	}

	void AddPoint(float3 p) {
		if (p.x < min.x) min.x = p.x;
		if (p.y < min.y) min.y = p.y;
		if (p.z < min.z) min.z = p.z;
		if (p.x > max.x) max.x = p.x;
		if (p.y > max.y) max.y = p.y;
		if (p.z > max.z) max.z = p.z;
	}
};

struct Ray {
	float3 origin     = {};
	float3 direction  = {};

	float3 Evaluate(float t) {
		return origin + direction * t;
	}
};


Plane operator*(const float4x4& mat, const Plane& plane);
inline Plane operator*(const Plane& plane, const float4x4& mat) { return mat * plane; }
bool Intersect(const AABB& a, const AABB& b);

float Intersect(const Ray& ray, const Plane& plane);
float Intersect(const Ray& ray, const AABB& aabb);
float IntersectTriangle(const Ray& ray, const float3& p1, const float3& p2, const float3& p3);
float DistanceToLineSegment(const Ray& ray, const float3& p1, const float3& p2, float* out_t1 = nullptr, float* out_t2 = nullptr);
float DistanceRayToLine(const Ray& ray, const float3& a, const float3& b, float* out_t1, float* out_t2);
float2 NearestPointToLineSegment(float2 a, float2 b, float2 point);