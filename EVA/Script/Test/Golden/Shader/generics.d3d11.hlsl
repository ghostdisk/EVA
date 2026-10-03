// VS (vertex)
struct S0
{
	float4x4 m0;
	float3x3 m1;
};

static const float g0[2][4] = { { 1.0, 2.0, 3.0, 4.0 }, { 5.0, 6.0, 7.0, 8.0 } };

float4 f0(uint p0, uint2 p1)
{
	uint l0 = (uint)0;
	uint2 l1 = (uint2)0;
	S0 l2 = (S0)0;
	float4 l3 = (float4)0;
	int3 l4[2] = (int3[2])0;
	l0 = p0;
	l1 = p1;
	float4 v0 = l3;
	uint v1 = l0;
	uint v2 = min(v1, 1u);
	uint v3 = l0;
	uint v4 = min(v3, 3u);
	float v5 = g0[v2][v4];
	float4 v6 = float4(v5, v5, v5, v5);
	float4 v7 = v0 + v6;
	return v7;
}

void main(uint in0 : ATTRIB0, uint2 in1 : ATTRIB1, out float4 out0 : SV_Position)
{
	uint v0 = in0;
	uint2 v1 = in1;
	float4 v2 = f0(v0, v1);
	out0 = v2;
}

// PS (fragment)
static const int3 g0[2] = { int3(1, 2, 3), int3(4, 4, 4) };

int4 f0(int3 p0)
{
	int3 l0 = (int3)0;
	l0 = p0;
	int3 v0 = l0;
	int3 v1 = g0[1u];
	int3 v2 = v0 + v1;
	int4 v3 = int4(v2, 1);
	return v3;
}

void main(nointerpolation int3 in0 : TEXCOORD0, out int4 out0 : SV_Target0)
{
	int3 v0 = in0;
	int4 v1 = f0(v0);
	out0 = v1;
}
