#pragma pack_matrix(row_major)

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjection;
	float4x4 ShadowMatrix;
};

cbuffer PSConstant : register(b0)
{
	float3 LightDirection;
}

#define FILTER_SIZE 2

Texture2D DiffuseTexture				: register(t0);
Texture2D ShadowMap						: register(t1);
SamplerState LinearSampler				: register(s0);
SamplerComparisonState ShadowSampler	: register(s1);


struct VertexOutput
{
	float2 tex			: TEXCOORD;
    float4 gl_Position	: SV_Position;
    float3 normal		: NORMAL;
	float4 ShadowCoord	: TEXCOORD1;
};

struct PixelOutput
{
    float4 outFragColor : SV_Target0;
};

float DoSampleCmp(float2 SubTexelCoord, float CurrentDepth, int2 TexelOffset)
{
	return ShadowMap.SampleCmpLevelZero(ShadowSampler, SubTexelCoord, CurrentDepth, TexelOffset);
}

float SampleFixedSizePCF(float3 ShadowPos, float3 LightDirection, float3 Normal)
{
	float shadow = 0.0f;

	float NumSlices;
	float2 ShadowMapSize;
	ShadowMap.GetDimensions(0, ShadowMapSize.x, ShadowMapSize.y, NumSlices);
	float2 texelSize = 1.0 / ShadowMapSize;

	for (float i = -2.5f; i < 3.0f; ++i) {
		for (float j = -2.5f; j < 3.0f; ++j) {
			int2 TexelOffset = int2(i, j);
			shadow += DoSampleCmp(ShadowPos.xy, ShadowPos.z, TexelOffset);
		}
	}

	shadow *= 0.04f;
	shadow = saturate(shadow);
	return shadow;
}


float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	float3 ShadowPos = ShadowCoord.xyz;
	return SampleFixedSizePCF(ShadowPos, -LightDirection, Normal);
}

PixelOutput ps_main(VertexOutput Input)
{
	PixelOutput output;
	float4 texColor = DiffuseTexture.Sample(LinearSampler, Input.tex);
	float Shadow = ComputeShadow(Input.ShadowCoord, Input.normal);	
	output.outFragColor = texColor * saturate(0.2 + Shadow);
	return output;
}