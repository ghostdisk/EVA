// VS (vertex)
float4 f0(float3 p0, float3 p1, int p2, uint2 p3)
{
	float3 l0 = (float3)0;
	float3 l1 = (float3)0;
	int l2 = (int)0;
	uint2 l3 = (uint2)0;
	float4x3 l4 = (float4x3)0;
	float3x4 l5 = (float3x4)0;
	float4x4 l6 = (float4x4)0;
	float4 l7 = (float4)0;
	float3 l8 = (float3)0;
	int l9 = (int)0;
	uint2 l10 = (uint2)0;
	float3 l11 = (float3)0;
	float3 l12 = (float3)0;
	float l13 = (float)0;
	l0 = p0;
	l1 = p1;
	l2 = p2;
	l3 = p3;
	float3x4 v0 = l5;
	float4x3 v1 = l4;
	float4x4 v2 = mul(v1, v0);
	l6 = v2;
	float3 v3 = l0;
	float4x3 v4 = l4;
	float4 v5 = mul(v4, v3);
	l7 = v5;
	float4x3 v6 = l4;
	float4 v7 = l7;
	float3 v8 = mul(v7, v6);
	l8 = v8;
	int v9 = l2;
	int v10 = min(v9, 3);
	int v11 = l2;
	int v12 = -1;
	int v13 = max(v11, v12);
	int v14 = v10 + v13;
	l9 = v14;
	uint2 v15 = l3;
	uint2 v16 = uint2(1u, 1u);
	uint2 v17 = max(v15, v16);
	l10 = v17;
	float3 v18 = l8;
	float3 v19 = l1;
	float3 v20 = float3(0.0, 0.0, 0.0);
	float3 v21 = max(v19, v20);
	float3 v22 = min(v18, v21);
	l11 = v22;
	float3 v23 = l1;
	float3 v24 = normalize(v23);
	l12 = v24;
	float3 v25 = l12;
	float3 v26 = float3(1.0, 2.0, 3.0);
	float3 v27 = normalize(v26);
	float v28 = dot(v25, v27);
	float v29 = max(v28, 0.0);
	l13 = v29;
	float4x4 v30 = l6;
	float3 v31 = l11;
	float v32 = l13;
	float3 v33 = float3(v32, v32, v32);
	float3 v34 = v31 * v33;
	float3 v35 = l0;
	float v36 = length(v35);
	float4 v37 = float4(v34, v36);
	float4 v38 = mul(v37, v30);
	return v38;
}

void main(float3 in0 : ATTRIB0, float3 in1 : ATTRIB1, int in2 : ATTRIB2, uint2 in3 : ATTRIB3, out float4 out0 : SV_Position)
{
	float3 v0 = in0;
	float3 v1 = in1;
	int v2 = in2;
	uint2 v3 = in3;
	float4 v4 = f0(v0, v1, v2, v3);
	out0 = v4;
}
