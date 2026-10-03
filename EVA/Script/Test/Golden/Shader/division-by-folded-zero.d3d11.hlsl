// PS (fragment)
float4 f0(int p0, uint p1, float p2)
{
	int l0 = (int)0;
	uint l1 = (uint)0;
	float l2 = (float)0;
	l0 = p0;
	l1 = p1;
	l2 = p2;
	int v0 = l0;
	int v1 = v0 / (0 == (int)0 ? (int)1 : 0);
	int v2 = l0;
	int v3 = l0;
	int v4 = l0;
	int v5 = v3 - v4;
	int v6 = v2 % (v5 == (int)0 ? (int)1 : v5);
	uint v7 = l1;
	uint v8 = l1;
	uint v9 = v8 * 0u;
	uint v10 = v7 / (v9 == (uint)0 ? (uint)1 : v9);
	uint v11 = l1;
	uint v12 = v11 % (0u == (uint)0 ? (uint)1 : 0u);
	float v13 = l2;
	float v14 = v13 / 0.0;
	float4 v15 = float4(v14, v14, v14, v14);
	return v15;
}

void main(nointerpolation int in0 : TEXCOORD0, nointerpolation uint in1 : TEXCOORD1, float in2 : TEXCOORD2, out float4 out0 : SV_Target0)
{
	int v0 = in0;
	uint v1 = in1;
	float v2 = in2;
	float4 v3 = f0(v0, v1, v2);
	out0 = v3;
}
