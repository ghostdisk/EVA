// VS (vertex)
struct S1
{
	float4 m0;
	uint m1;
};

struct S0
{
	float4 m0;
	float2 m1;
	S1 m2;
};

void f0(uint p0, float2 p1, int p2, inout S0 p3)
{
	uint l0 = (uint)0;
	float2 l1 = (float2)0;
	int l2 = (int)0;
	l0 = p0;
	l1 = p1;
	l2 = p2;
	float2 v0 = l1;
	float4 v1 = float4(v0, 0.0, 1.0);
	p3.m0 = v1;
	float2 v2 = l1;
	p3.m1 = v2;
	float4 v3 = float4(1.0, 1.0, 1.0, 1.0);
	p3.m2.m0 = v3;
	uint v4 = l0;
	p3.m2.m1 = v4;
}

void main(int in0 : ATTRIB0, float2 in1 : ATTRIB3, uint in2 : SV_VertexID, out float2 out0 : TEXCOORD0, out float4 out1 : TEXCOORD1, nointerpolation out uint out2 : TEXCOORD2, out float4 out3 : SV_Position)
{
	S0 l0 = (S0)0;
	uint v0 = in2;
	float2 v1 = in1;
	int v2 = in0;
	f0(v0, v1, v2, l0);
	float4 v3 = l0.m0;
	out3 = v3;
	float2 v4 = l0.m1;
	out0 = v4;
	float4 v5 = l0.m2.m0;
	out1 = v5;
	uint v6 = l0.m2.m1;
	out2 = v6;
}

// PS (fragment)
struct S1
{
	float4 m0;
	uint m1;
};

struct S0
{
	float4 m0;
	float2 m1;
	S1 m2;
};

float4 f0(inout S0 p0)
{
	float4 v0 = p0.m2.m0;
	return v0;
}

void main(float2 in0 : TEXCOORD0, float4 in1 : TEXCOORD1, nointerpolation uint in2 : TEXCOORD2, float4 in3 : SV_Position, out float4 out0 : SV_Target0)
{
	S0 l0 = (S0)0;
	float4 v0 = in3;
	l0.m0 = v0;
	float2 v1 = in0;
	l0.m1 = v1;
	float4 v2 = in1;
	l0.m2.m0 = v2;
	uint v3 = in2;
	l0.m2.m1 = v3;
	float4 v4 = f0(l0);
	out0 = v4;
}
