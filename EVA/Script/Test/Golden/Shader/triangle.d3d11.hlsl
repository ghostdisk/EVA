// VSMain (vertex)
static const float2 g0[3] = { float2(0.0, 0.5), float2(0.5, (-0.5)), float2((-0.5), (-0.5)) };

float4 f0(uint p0)
{
	uint l0 = (uint)0;
	l0 = p0;
	uint v0 = l0;
	uint v1 = min(v0, 2u);
	float2 v2 = g0[v1];
	float4 v3 = float4(v2, 0.0, 1.0);
	return v3;
}

void main(uint in0 : SV_VertexID, out float4 out0 : SV_Position)
{
	uint v0 = in0;
	float4 v1 = f0(v0);
	out0 = v1;
}

// PSMain (fragment)
float4 f0()
{
	float4 v0 = float4(1.0, 1.0, 1.0, 1.0);
	return v0;
}

void main(out float4 out0 : SV_Target0)
{
	float4 v0 = f0();
	out0 = v0;
}
