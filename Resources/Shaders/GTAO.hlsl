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
	float4x4	ViewMatrix;
	float4		Resolution;			// width height 1/width 1/height
	float4		ClipInfo;
	float2		Params;				// angle and offset
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

static const float	c_NumAngles = 8.0f;
static const uint	GTAO_NUMTAPS = 10;

float3 ReconstructViewPos(float2 Tex)
{
#if 0
	float Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).r;
	float2 ScreenCoord = ViewportUVToScreenPos(Tex);
	float4 NDCPos = float4(ScreenCoord, Depth, 1.0f);
	float4 ViewdPos = mul(NDCPos, InvProjMatrix);
	ViewdPos /= ViewdPos.w;
	return ViewdPos.xyz;
#else
	float Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).r;
	float2 ScreenCoord = ViewportUVToScreenPos(Tex);
	float Z = LinearEyeDepth(Depth, ClipInfo.x, ClipInfo.y);
	float2 XY = ScreenCoord * ClipInfo.zw * Z;
	return float3(XY, Z);
#endif
}

float2 SearchForLargestAngleDual(uint NumSteps, float2 BaseUV, float2 ScreenDir, float3 ViewPos, float3 ViewDir)
{
	float LenDsSquare, LenDsInv, Ang, FallOff;
	float3 Ds;
	float2 BestAng = float2(-1, -1);

	const float WorldRadius = 30.0f;
	float AttenFactor = 2.0 / (WorldRadius * WorldRadius);

	float Thickness = 0.9f;

	for (uint i = 0; i < NumSteps; i++)
	{
		float fi = (float)i;

		float2 UVOffset = ScreenDir * (fi + 1.0) * Resolution.zw;
		// why?
		UVOffset.y *= -1;

		float4 UV2 = BaseUV.xyxy + float4(UVOffset.xy, -UVOffset.xy);

		// h1
		Ds = ReconstructViewPos(UV2.xy) - ViewPos;
		LenDsSquare = dot(Ds, Ds);
		LenDsInv = rsqrt(LenDsSquare + 0.0001);
		Ang = dot(Ds, ViewDir) * LenDsInv;

		FallOff = saturate(LenDsSquare * AttenFactor);
		Ang = lerp(Ang, BestAng.x, FallOff);

		BestAng.x = (Ang > BestAng.x) ? Ang : lerp(Ang, BestAng.x, Thickness);

		// h2
		Ds = ReconstructViewPos(UV2.zw) - ViewPos;
		LenDsSquare = dot(Ds, Ds);
		LenDsInv = rsqrt(LenDsSquare + 0.0001);
		Ang = dot(Ds, ViewDir) * LenDsInv;

		FallOff = saturate(LenDsSquare * AttenFactor);
		Ang = lerp(Ang, BestAng.x, FallOff);

		BestAng.y = (Ang > BestAng.y) ? Ang : lerp(Ang, BestAng.y, Thickness);
	}
	BestAng.x = acos(clamp(BestAng.x, -1.0, 1.0));
	BestAng.y = acos(clamp(BestAng.y, -1.0, 1.0));

	return BestAng;
}

float ComputeInnerIntegral(float2 Angles, float2 ScreenDir, float3 ViewDir, float3 ViewSpaceNormal)
{
	// Given the angles found in the search plane 
	// we need to project the View Space Normal onto the plane 
	// defined by the search axis and the View Direction and perform the inner integrate
	float3 PlaneNormal = normalize(cross(float3(ScreenDir, 0), ViewDir));
	float3 Perp = cross(ViewDir, PlaneNormal);
	float3 ProjNormal = ViewSpaceNormal - PlaneNormal * dot(ViewSpaceNormal, PlaneNormal);

	float LenProjNormal = length(ProjNormal) + 0.000001f;
	float RecipMag = 1.0f / (LenProjNormal);

	float CosAng = dot(ProjNormal, Perp) * RecipMag;
	float Gamma = acos(CosAng) - HALF_PI;
	float CosGamma = dot(ProjNormal, ViewDir) * RecipMag;
	float SinGamma = CosAng * -2.0f;

	// clamp to normal hemisphere 
	Angles.x = Gamma + max(-Angles.x - Gamma, -(HALF_PI));
	Angles.y = Gamma + min(Angles.y - Gamma, (HALF_PI));

	float AO = ((LenProjNormal) * 0.25 *
			((Angles.x * SinGamma + CosGamma - cos((2.0 * Angles.x) - Gamma)) +
			(Angles.y * SinGamma + CosGamma - cos((2.0 * Angles.y) - Gamma))));

	return AO;
}

float MutiBounce(float AO, float3 albedo)
{
	float3 a = 2.0404 * albedo - 0.3324;
	float3 b = -4.7951 * albedo + 0.6417;
	float3 c = 2.7552 * albedo + 0.6903;

	return max(AO, ((AO * a + b) * AO + c) * AO);
}

PixelOutput PS_GTAO(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);
	
	// view normal
	float3 WorldNormal = GBufferA.SampleLevel(LinearSampler, Tex, 0).rgb;
	WorldNormal = WorldNormal * 2 - 1;
	float3 ViewSpaceNormal = normalize(mul(WorldNormal, (float3x3)ViewMatrix));

 	// view position
	float3 ViewSpacePos = ReconstructViewPos(Tex);
	float3 ViewDir = normalize(-ViewSpacePos);

	float Sum = 0.0;

	const float phi = Params.x * PI;
	float2 ScreenDir = float2(cos(phi), sin(phi));

	uint NumAngles = (uint)c_NumAngles;
	float SinDeltaAngle = sin(PI / c_NumAngles);
	float CosDeltaAngle = cos(PI / c_NumAngles);

	for (uint Angle = 0; Angle < NumAngles; Angle++)
	{
		float2 horizons = SearchForLargestAngleDual(GTAO_NUMTAPS, Tex, ScreenDir, ViewSpacePos, ViewDir);

		Sum += ComputeInnerIntegral(horizons, ScreenDir, ViewDir, ViewSpaceNormal);

		// Rotate for the next angle
		float2 TempScreenDir = ScreenDir.xy;
		ScreenDir.x = (TempScreenDir.x * CosDeltaAngle) + (TempScreenDir.y * -SinDeltaAngle);
		ScreenDir.y = (TempScreenDir.x * SinDeltaAngle) + (TempScreenDir.y * CosDeltaAngle);
	}

	float AO = Sum;
	AO = AO / ((float)NumAngles);

	Out.Target0 = float4(AO, AO, AO, 1);
	return Out;
}