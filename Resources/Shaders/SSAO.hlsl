#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D			GBufferA		: register(t0);		// Normal
Texture2D			GBufferC		: register(t1);		// DiffuseAO
Texture2D<float>	SceneDepthZ		: register(t2);		// Depth
Texture2D			NoiseMap		: register(t3);		// Noise

SamplerState LinearSampler		: register(s0);
SamplerState WrapLinearSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float4x4	InvViewProjMatrix;
	float4x4	ViewProjMatrix;
	float4		Samples[64];
	float		Radius;
	float		Near;
	float		Far;
};

static const int kernelSize = 64;
static const float2 NoiseScale = float2(1024.0f / 4.0f, 768.0f / 4.0f);

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

PixelOutput PS_SSAO(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	// World Pos
	float Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).r;
	float2 ScreenCoord = ViewportUVToScreenPos(Tex);
	float4 NDCPos = float4(ScreenCoord, Depth, 1.0f);
	float4 WorldPos = mul(NDCPos, InvViewProjMatrix);
	WorldPos /= WorldPos.w;

	// World Normal
	float3 N = GBufferA.SampleLevel(LinearSampler, Tex, 0).rgb;
	N = N * 2 - 1.0;
	
	// Sample Random Vector Need Scale
	float3 RandomVec = NoiseMap.SampleLevel(WrapLinearSampler, Tex * NoiseScale, 0).rgb;
	// TBN
	float3 T = normalize(RandomVec - N * dot(RandomVec, N));
	float3 B = cross(N, T); 
	float3x3 TBN = float3x3(T, B, N);

	float Occlusion = 0.0f;
	for (int i = 0; i < kernelSize; ++i)
	{
		// Tangent Space to World Space
		float3 Sample = mul(Samples[i].xyz, (float3x3)TBN);
		Sample = WorldPos + Sample * Radius;

		float4 Offset = float4(Sample, 1.0);
		// View and Projection
		Offset = mul(Offset, ViewProjMatrix);
		Offset.xyz /= Offset.w;

		float2 SampleUV = ScreenPosToViewportUV(Offset.xy);
		float SampleDepth = SceneDepthZ.SampleLevel(LinearSampler, SampleUV, 0).r;
		
		Occlusion += (SampleDepth < Offset.z ? 1.0 : 0.0);
	}

	Occlusion /= kernelSize;
	// Unocclusion
	Occlusion = 1 - Occlusion;
	Out.Target0 = float4(Occlusion, Occlusion, Occlusion, 1);
	return Out;
}


