#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"
#include "PixelPacking_Velocity.hlsli"

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
	float4x4 PreviousModelMatrix;
	float4x4 PreviousViewProjMatrix;
	float2	 ViewportSize;
};

cbuffer PSContant : register(b0)
{
	float	Exposure;
	float3	CameraPos;

	float3	BaseColor;
	float	Metallic;
	
	float	Roughness;
	int		MaxMipLevel;
	int		bSHDiffuse;
	int		Degree;

	float4x4 InvViewProj;
	
	float3	Coeffs[16];
};


Texture2D BaseMap 			: register(t0);
Texture2D OpacityMap 		: register(t1);
Texture2D EmissiveMap 		: register(t2);
Texture2D MetallicMap 		: register(t3);
Texture2D RoughnessMap 		: register(t4);
Texture2D AOMap 			: register(t5);
Texture2D NormalMap 		: register(t6);
TextureCube IrradianceCubeMap : register(t7);
TextureCube PrefilteredCubeMap: register(t8);
Texture2D PreintegratedGF 	: register(t9);

SamplerState LinearSampler	: register(s0);


struct VertexInput
{
	float3 Position : POSITION;
	float2 Tex		: TEXCOORD;
	float3 Normal	: NORMAL;
	float4 Tangent	: TANGENT;
};

struct PixelInput
{
	float4 Position	: SV_Position;
	float2 Tex		: TEXCOORD0;
	float3 T		: TEXCOORD1;
	float3 B		: TEXCOORD2;
	float3 N		: TEXCOORD3;
	float3 WorldPos	: TEXCOORD4;
	float3 PreviousScreenPos : TEXCOORD5;
	float3 CurrentScreenPos  : TEXCOORD6;
};

struct PixelOutput
{
	float4 Target0 : SV_Target0;
	float4 Target1 : SV_Target1;
	float4 Target2 : SV_Target2;
	float4 Target3 : SV_Target3;
	float2 Target4 : SV_Target4;
};

PixelInput VS_PBR(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	float4 PreviousWorldPos = mul(float4(In.Position, 1.0), PreviousModelMatrix); 
	float4 ClipPos = mul(PreviousWorldPos, PreviousViewProjMatrix);
	ClipPos /= ClipPos.w;
	Out.PreviousScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.PreviousScreenPos.y = 1 - Out.PreviousScreenPos.y;
	Out.PreviousScreenPos.xy *= ViewportSize;
	Out.PreviousScreenPos.z = ClipPos.z;

	float4 WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	ClipPos = mul(WorldPos, ViewProjMatrix);

	Out.WorldPos = WorldPos.xyz;
	Out.Position = ClipPos;

	ClipPos /= ClipPos.w;
	Out.CurrentScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.CurrentScreenPos.y = 1 - Out.CurrentScreenPos.y;
	Out.CurrentScreenPos.xy *= ViewportSize;
	Out.CurrentScreenPos.z = ClipPos.z;

	//Out.WorldPos = WorldPos.xyz;
	//Out.Position = mul(float4(Out.WorldPos, 1), ViewProjMatrix);

	Out.N = mul(In.Normal, (float3x3)ModelMatrix);
	Out.T = mul(In.Tangent.xyz, (float3x3)ModelMatrix);
	Out.B = cross(In.Normal, In.Tangent.xyz) * In.Tangent.w;
	Out.B = mul(Out.B, (float3x3)ModelMatrix);
	
	return Out;
}

PixelInput VS_PBR_Floor(in uint VertID : SV_VertexID)
{
	float3 Positions[6] = {
		float3(-1.0, 0.0, 1.0),   //0
		float3(1.0,  0.0, 1.0),   //1
		float3(-1.0, 0.0, -1.0),  //3
		float3(1.0,  0.0, 1.0),	  //1
		float3(1.0,  0.0, -1.0),  //2
		float3(-1.0, 0.0, -1.0),  //3
	};
	float2 Texs[6] = {
		float2(0.0, 0.0),   //0
		float2(1.0, 0.0),   //1
		float2(0.0, 1.0),   //3
		float2(1.0, 0.0),   //1
		float2(1.0, 1.0),   //2
		float2(0.0, 1.0),   //3
	};

	float3 InPosition = Positions[VertID];
	float3 InNormal = float3(0.0, 1.0, 0.0);
	float3 InTangent = float3(0.0, 0.0, 1.0);

	PixelInput Out;
	Out.Tex = Texs[VertID];

	float4 PreviousWorldPos = mul(float4(InPosition, 1.0), PreviousModelMatrix);
	float4 ClipPos = mul(PreviousWorldPos, PreviousViewProjMatrix);
	ClipPos /= ClipPos.w;
	Out.PreviousScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.PreviousScreenPos.y = 1 - Out.PreviousScreenPos.y;
	Out.PreviousScreenPos.xy *= ViewportSize;
	Out.PreviousScreenPos.z = ClipPos.z;

	float4 WorldPos = mul(float4(InPosition, 1.0), ModelMatrix);
	ClipPos = mul(WorldPos, ViewProjMatrix);

	Out.WorldPos = WorldPos.xyz;
	Out.Position = ClipPos;

	ClipPos /= ClipPos.w;
	Out.CurrentScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.CurrentScreenPos.y = 1 - Out.CurrentScreenPos.y;
	Out.CurrentScreenPos.xy *= ViewportSize;
	Out.CurrentScreenPos.z = ClipPos.z;

	//Out.WorldPos = WorldPos.xyz;
	//Out.Position = mul(float4(Out.WorldPos, 1), ViewProjMatrix);

	Out.N = mul(InNormal, (float3x3)ModelMatrix);
	Out.T = mul(InTangent.xyz, (float3x3)ModelMatrix);
	Out.B = cross(InNormal, InTangent.xyz);
	Out.B = mul(Out.B, (float3x3)ModelMatrix);

	return Out;
}

float4 visualizeVec(float3 v)
{
	float3 vv = (v + 1) / 2;
	return float4(vv, 1.0);
}


float3 F_schlickR(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


float3 CalcIBL(float3 N, float3 V, float3 Albedo, float Metallic, float Roughness, float AO)
{
	float3 R = reflect(-V, N); //incident ray, surface normal

	float NoV = saturate(dot(N, V));
	float3 F0 = lerp(0.04, Albedo.rgb, Metallic);
	float3 F = F_schlickR(NoV, F0, Roughness);

	float3 kD = (1.0 - F) * (1.0 - Metallic);

	float3 Irradiance = 0;
	if (bSHDiffuse)
	{
		// SH Irradiance
		Irradiance = GetSHIrradiance(N, Degree, Coeffs);
	}
	else
	{
		Irradiance = IrradianceCubeMap.SampleLevel(LinearSampler, N, 0).xyz;
	}

	float3 Diffuse = Albedo * kD * Irradiance;

	float Mip = ComputeReflectionCaptureMipFromRoughness(Roughness, MaxMipLevel - 1);
	float2 BRDF = PreintegratedGF.Sample(LinearSampler, float2(NoV, Roughness)).rg;

	float3 PrefilteredColor = PrefilteredCubeMap.SampleLevel(LinearSampler, R, Mip).rgb;
	float3 Specular = PrefilteredColor * (F * BRDF.x + BRDF.y);

	return (Diffuse + Specular) * AO;
}


void PS_PBR_Floor(PixelInput In, out PixelOutput Out)
{
	float3 Color = BaseMap.Sample(LinearSampler, In.Tex).xyz;
	float Alpha = OpacityMap.Sample(LinearSampler, In.Tex).r;

	float3 N = normalize(In.N);
	
	Out.Target0 = float4(0.0, 0.0, 0.0, 1.0);
	Out.Target1 = float4(0.5 * N + 0.5, 1.0);
	Out.Target2 = float4(Metallic, 0.5, Roughness, 1.0);
	Out.Target3 = float4(BaseColor, 1.0);
	Out.Target4 = float2(0.0, 0.0);
}


void PS_PBR(PixelInput In, out PixelOutput Out)
{
	float Opacity = OpacityMap.Sample(LinearSampler, In.Tex).r;
	clip(Opacity < 0.1f ? -1 : 1);

	float3 Albedo = BaseMap.Sample(LinearSampler, In.Tex).xyz;
	float Metallic = MetallicMap.Sample(LinearSampler, In.Tex).x;
	float Roughness = RoughnessMap.Sample(LinearSampler, In.Tex).x;
	float AO = AOMap.Sample(LinearSampler, In.Tex).x;
	float3 Emissive = EmissiveMap.Sample(LinearSampler, In.Tex).xyz;

	float3x3 TBN = float3x3(normalize(In.T), normalize(In.B), normalize(In.N));
	float3 tNormal = NormalMap.Sample(LinearSampler, In.Tex).xyz;
	tNormal = 2 * tNormal - 1.0; // [0,1] -> [-1, 1]
	float3 N = mul(tNormal, TBN);

	Out.Target0 = float4(Emissive, 1.0);
	Out.Target1 = float4(0.5*N+0.5, 1.0);
	Out.Target2 = float4(Metallic, 0.5, Roughness, 1.0);
	Out.Target3 = float4(Albedo, AO);
	Out.Target4 = In.PreviousScreenPos.xy - In.CurrentScreenPos.xy; // velocity
}

Texture2D GBufferA		: register(t0); // normal
Texture2D GBufferB		: register(t1); // metallSpecularRoughness
Texture2D GBufferC		: register(t2); // AlbedoAO
Texture2D SceneDepthZ	: register(t3); // Depth

float4 PS_IBL(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float3 PBRParameters = GBufferB.Sample(LinearSampler, Tex).xyz;
	float Metallic = PBRParameters.x;
	float Roughness = PBRParameters.z;

	float4 AlbedoAo = GBufferC.Sample(LinearSampler, Tex);
	float AO = AlbedoAo.w;

	float Depth = SceneDepthZ.Sample(LinearSampler, Tex).x;
	float2 ScreenCoord = float2(2.0, -2.0) * Tex + (-1.0, 1.0);
	float4 NDCPos = float4(ScreenCoord, Depth, 1.0f);
	float4 WorldPos = mul(NDCPos, InvViewProj);
	WorldPos /= WorldPos.w;

	float3 V = normalize(CameraPos - WorldPos.xyz);
	float3 IBL = CalcIBL(N, V, AlbedoAo.xyz, Metallic, Roughness, AO);

	return float4(IBL, 1.0);
}