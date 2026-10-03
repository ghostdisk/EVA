// VS (vertex)
static const float4 g0[3] = { float4(1.0, 1.0, 1.0, 1.0), float4(2.0, 2.0, 2.0, 2.0), float4(3.0, 3.0, 3.0, 3.0) };

float4 f0(uint p0, int p1)
{
	uint l0 = (uint)0;
	int l1 = (int)0;
	float4 l2[2] = (float4[2])0;
	l0 = p0;
	l1 = p1;
	uint v0 = l0;
	uint v1 = v0 * 0u;
	uint v2 = v1 + 5u;
	uint v3 = min(v2, 2u);
	float4 v4 = g0[v3];
	int v5 = l1;
	int v6 = l1;
	int v7 = v5 - v6;
	int v8 = v7 - 1;
	uint v9 = (uint)v8;
	uint v10 = min(v9, 2u);
	float4 v11 = g0[v10];
	float4 v12 = v4 + v11;
	uint v13 = l0;
	uint v14 = v13 + 7u;
	uint v15 = min(v14, 1u);
	float4 v16 = l2[v15];
	float4 v17 = v12 + v16;
	return v17;
}

void main(int in0 : ATTRIB0, uint in1 : SV_VertexID, out float4 out0 : SV_Position)
{
	uint v0 = in1;
	int v1 = in0;
	float4 v2 = f0(v0, v1);
	out0 = v2;
}
