// PS (fragment)
struct S0
{
	int m0;
	uint m1;
};

struct S2
{
	uint m0;
	float2 m1[2];
};

struct S1
{
	S2 m0[2];
	int m1;
	float4 m2;
};

static const int g0[2][3] = { { 1, 2, 3 }, { (-2147483647 - 1), 0, 2147483647 } };
static const S1 g1 = { { { 1u, { float2(1.0, 2.0), float2(3.0, 4.0) } }, { 2u, { float2(5.0, 5.0), float2(6.0, 6.0) } } }, (-7), float4(asfloat(0x00000001u), asfloat(0x006CE3EEu), 3.4e+38, asfloat(0x80000000u)) };

void f0(uint p0, inout S0 p1)
{
	uint l0 = (uint)0;
	l0 = p0;
	uint v0 = l0;
	uint v1 = min(v0, 1u);
	uint v2 = l0;
	uint v3 = min(v2, 2u);
	int v4 = g0[v1][v3];
	int v5 = g1.m1;
	uint v6 = l0;
	uint v7 = min(v6, 2u);
	int v8 = g0[1u][v7];
	int v9 = v5 * v8;
	int v10 = v4 + v9;
	p1.m0 = v10;
	uint v11 = l0;
	uint v12 = min(v11, 1u);
	uint v13 = g1.m0[v12].m0;
	p1.m1 = v13;
}

void main(nointerpolation uint in0 : TEXCOORD0, out int out0 : SV_Target0, out uint out1 : SV_Target1)
{
	S0 l0 = (S0)0;
	uint v0 = in0;
	f0(v0, l0);
	int v1 = l0.m0;
	out0 = v1;
	uint v2 = l0.m1;
	out1 = v2;
}

// VS (vertex)
struct S1
{
	uint m0;
	float2 m1[2];
};

struct S0
{
	S1 m0[2];
	int m1;
	float4 m2;
};

static const S0 g0 = { { { 1u, { float2(1.0, 2.0), float2(3.0, 4.0) } }, { 2u, { float2(5.0, 5.0), float2(6.0, 6.0) } } }, (-7), float4(asfloat(0x00000001u), asfloat(0x006CE3EEu), 3.4e+38, asfloat(0x80000000u)) };

float4 f0(uint p0)
{
	uint l0 = (uint)0;
	l0 = p0;
	uint v0 = l0;
	uint v1 = min(v0, 1u);
	uint v2 = l0;
	uint v3 = min(v2, 1u);
	float2 v4 = g0.m0[v1].m1[v3];
	float4 v5 = float4(v4, 0.0, 1.0);
	float4 v6 = g0.m2;
	float4 v7 = v5 + v6;
	return v7;
}

void main(uint in0 : ATTRIB0, out float4 out0 : SV_Position)
{
	uint v0 = in0;
	float4 v1 = f0(v0);
	out0 = v1;
}
