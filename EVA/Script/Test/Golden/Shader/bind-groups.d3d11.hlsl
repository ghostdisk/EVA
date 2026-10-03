// VS (vertex)
#pragma pack_matrix(row_major)

struct S1
{
	float3 m0;
	float m1;
	float3 m2;
};

struct S0
{
	float4x4 m0;
	float3 m1;
	float m2;
	float2 m3;
	float m4;
	float3x3 m5;
	float m6;
	S1 m7[2];
	uint m8;
	float m9[3];
	int2 m10;
};

cbuffer B0 : register(b0)
{
	S0 g0;
};

float4 f0(float3 p0)
{
	float3 l0 = (float3)0;
	float4 l1 = (float4)0;
	l0 = p0;
	float3 v0 = l0;
	float v1 = g0.m6;
	float3 v2 = float3(v1, v1, v1);
	float3 v3 = v0 * v2;
	float4 v4 = float4(v3, 1.0);
	l1 = v4;
	float4x4 v5 = g0.m0;
	float4 v6 = l1;
	float4 v7 = mul(v6, v5);
	float3 v8 = g0.m7[1u].m2;
	float v9 = g0.m9[2u];
	float4 v10 = float4(v8, v9);
	float4 v11 = v7 + v10;
	return v11;
}

void main(float3 in0 : ATTRIB0, out float4 out0 : SV_Position)
{
	float3 v0 = in0;
	float4 v1 = f0(v0);
	out0 = v1;
}

// PS (fragment)
#pragma pack_matrix(row_major)

struct S1
{
	float m0;
	float m1;
};

struct S2
{
	float3 m0;
	float m1;
	float3 m2;
};

struct S0
{
	float m0;
	S1 m1;
	float m2;
	float3 m3;
	S2 m4;
	uint2 m5;
};

cbuffer B2 : register(b1)
{
	S0 g0;
};

float4 f0()
{
	float3 v0 = g0.m3;
	float v1 = g0.m0;
	float4 v2 = float4(v0, v1);
	return v2;
}

void main(out float4 out0 : SV_Target0)
{
	float4 v0 = f0();
	out0 = v0;
}
