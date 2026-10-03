// VS (vertex)
struct S0
{
	float4 m0[4];
	uint m1;
};

float4 f0(uint p0)
{
	uint l0 = (uint)0;
	S0 l1 = (S0)0;
	float4 l2[2][2] = (float4[2][2])0;
	l0 = p0;
	uint v0 = l0;
	uint v1 = min(v0, 3u);
	float4 v2 = l1.m0[v1];
	uint v3 = l0;
	uint v4 = min(v3, 1u);
	float4 v5 = l2[v4][1u];
	float4 v6 = v2 + v5;
	return v6;
}

void main(uint in0 : SV_VertexID, out float4 out0 : SV_Position)
{
	uint v0 = in0;
	float4 v1 = f0(v0);
	out0 = v1;
}
