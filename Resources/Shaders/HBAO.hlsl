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
	float4x4	InvProjMatrix;
	float4		Resolution;			// width height 1/width 1/height
	float		NumDirections;
	float		NumSamples;
	float		TraceRadius;
	float		MaxRadiusPixels;
	float2		FocalLen;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

float3 GetViewPos(float2 Tex)
{
	float Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).r;
	float2 ScreenCoord = ViewportUVToScreenPos(Tex);
	float4 NDCPos = float4(ScreenCoord, Depth, 1.0f);
	float4 ViewdPos = mul(NDCPos, InvProjMatrix);
	ViewdPos /= ViewdPos.w;
	return ViewdPos.xyz;
}

float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
	float3 V1 = Pr - P;
	float3 V2 = P - Pl;
	return (length(V1) < length(V2)) ? V1 : V2;
}

void ComputeSteps(out float2 stepSizeUv, out float numSteps, float rayRadiusPix, float rand)
{
	numSteps = min(NumSamples, rayRadiusPix);

	float stepSizePix = rayRadiusPix / (numSteps + 1);

	float maxNumSteps = MaxRadiusPixels / stepSizePix;
	if (maxNumSteps < numSteps)
	{
		numSteps = floor(maxNumSteps + rand);
		numSteps = max(numSteps, 1);
	}

	stepSizeUv = stepSizePix * Resolution.zw;
}

float2 RotateDirections(float2 Dir, float2 CosSin)
{
	// https://zhuanlan.zhihu.com/p/58517426
	return float2(
		Dir.x * CosSin.x - Dir.y * CosSin.y,
		Dir.x * CosSin.y + Dir.y * CosSin.x);
}

float Length2(float3 V)
{
	return dot(V, V);
}

float InvLength(float2 V)
{
	return 1.0f / sqrt(dot(V, V));
}

float Tangent(float3 V)
{
	// in D3D, z is negative towards eye, so inverse it
	return -V.z * InvLength(V.xy);
}

float Tangent(float3 P, float3 S)
{
	return Tangent(S - P);
}

float BiasedTangent(float3 V)
{
	const float TanBias = tan(30.0 * PI / 180.0);
	return Tangent(V) + TanBias;
}

float TanToSin(float x)
{
	return x / sqrt(x * x + 1.0);
}

float Falloff(float d2)
{
	// The farther the distance, the smaller the contribution
	return saturate(1.0f - d2 * 1.0 / (TraceRadius * TraceRadius));
}

float2 SnapUVOffset(float2 uv)
{
	// Rounds the specified value to the nearest integer.
	return round(uv * Resolution.xy) * Resolution.zw;
}

float HorizonOcclusion(
	float2	Tex,
	float2	deltaUV,
	float3	P,
	float3	dPdu,
	float3	dPdv,
	float	randstep,
	float	numSamples)
{
	float ao = 0;

	float2 uv = Tex + SnapUVOffset(randstep * deltaUV);

	deltaUV = SnapUVOffset(deltaUV);

	float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

	float tanH = BiasedTangent(T);
	float sinH = TanToSin(tanH);

	float tanS;
	float d2;
	float3 S;

	for (float s = 1; s <= numSamples; ++s)
	{
		uv += deltaUV;
		S = GetViewPos(uv);
		
		// p as the origin of the space
		tanS = Tangent(P, S);
		d2 = Length2(S - P);

		// only above Tangent can make contribution
		if (d2 < TraceRadius * TraceRadius && tanS > tanH)
		{
			float sinS = TanToSin(tanS);
			ao += Falloff(d2) * (sinS - sinH);
			tanH = tanS;
			sinH = sinS;
		}
	}
	return ao;
}


PixelOutput PS_HBAO(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	const float2 NoiseScale = float2(Resolution.x / 4.0f, Resolution.y / 4.0f);

	// view position 
	float3 P = GetViewPos(Tex);
	float3 Pl = GetViewPos(Tex + float2(-Resolution.z, 0));
	float3 Pr = GetViewPos(Tex + float2(Resolution.z, 0));
	float3 Pt = GetViewPos(Tex + float2(0, Resolution.w));
	float3 Pb = GetViewPos(Tex + float2(0, -Resolution.w));

	// used to calculate tangent
	float3 dPdu = MinDiff(P, Pr, Pl);
	float3 dPdv = MinDiff(P, Pt, Pb) * (Resolution.y * 1.0 / Resolution.x);

	// sample random vector need scale
	float3 RandomVec = NoiseMap.SampleLevel(WrapLinearSampler, Tex * NoiseScale, 0).rgb;

	// The 0.5 uv range corresponds to the camera fov angle
	// FocalLen = 1 / tan(theta/2)
	float2 rayRadiusUV = 0.5 * FocalLen * TraceRadius / P.z;
	// radius in pixels
	float rayRadiusPix = rayRadiusUV.x * Resolution.x;

	float ao = 1.0;

	float numSteps;
	float2 stepSizeUV;

	if (rayRadiusPix > 1.0)
	{
		ao = 0.0;
		ComputeSteps(stepSizeUV, numSteps, rayRadiusPix, RandomVec.z);

		float alpha = 2.0 * PI / NumDirections;
		for (float i = 0; i < NumDirections; ++i)
		{
			float theta = alpha * i;
			float2 dir = RotateDirections(float2(cos(theta), sin(theta)), RandomVec.xy);
			float2 deltaUV = dir * stepSizeUV;

			// accumulate occlusion
			ao += HorizonOcclusion(
				Tex,
				deltaUV,
				P,
				dPdu,
				dPdv,
				RandomVec.z,
				numSteps);
		}
		ao = 1.0f - ao / NumDirections;
	}
	Out.Target0 = ao;
	return Out;
}