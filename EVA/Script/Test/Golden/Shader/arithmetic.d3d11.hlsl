// PS (fragment)
float4 f0(int p0, uint p1, float4 p2)
{
	int l0 = (int)0;
	uint l1 = (uint)0;
	float4 l2 = (float4)0;
	l0 = p0;
	l1 = p1;
	l2 = p2;
	int v0 = l0;
	int v1 = l0;
	int v2 = 2 * v1;
	int v3 = v0 + v2;
	int v4 = l0;
	int v5 = v4 / 3;
	int v6 = l0;
	int v7 = v5 % (v6 == (int)0 ? (int)1 : v6);
	int v8 = v3 - v7;
	int v9 = l0;
	int v10 = -v9;
	int v11 = v8 - v10;
	uint v12 = l1;
	uint v13 = l1;
	uint v14 = v12 / (v13 == (uint)0 ? (uint)1 : v13);
	uint v15 = l1;
	uint v16 = v15 % 7u;
	uint v17 = v14 + v16;
	uint v18 = l1;
	uint v19 = l1;
	uint v20 = v18 * v19;
	uint v21 = v17 - v20;
	float4 v22 = l2;
	float4 v23 = -v22;
	float4 v24 = l2;
	float4 v25 = v23 * v24;
	float4 v26 = l2;
	float4 v27 = v25 / v26;
	float4 v28 = l2;
	float4 v29 = v27 % v28;
	float4 v30 = float4(1.5e-30, 1.5e-30, 1.5e-30, 1.5e-30);
	float4 v31 = v29 - v30;
	float4 v32 = float4(3.4e+38, 3.4e+38, 3.4e+38, 3.4e+38);
	float4 v33 = v31 + v32;
	return v33;
}

void main(nointerpolation int in0 : TEXCOORD0, nointerpolation uint in1 : TEXCOORD1, float4 in2 : TEXCOORD2, out float4 out0 : SV_Target0)
{
	int v0 = in0;
	uint v1 = in1;
	float4 v2 = in2;
	float4 v3 = f0(v0, v1, v2);
	out0 = v3;
}
