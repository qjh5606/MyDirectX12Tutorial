#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
	float4x4 InverseTransposeModelMatrix;
};

cbuffer PSContant : register(b0)
{
	float		Exposure;
	float3		CameraPos;
	float4x4 	InvViewProj;
};


Texture2D BaseMap 			: register(t0);
Texture2D OpacityMap 		: register(t1);
Texture2D EmissiveMap 		: register(t2);
Texture2D MetallicMap 		: register(t3);
Texture2D RoughnessMap 		: register(t4);
Texture2D AOMap 			: register(t5);
Texture2D NormalMap 		: register(t6);

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
};

struct PixelOutput
{
	float4 Target0 : SV_Target0;
	float4 Target1 : SV_Target1;	// Normal
	float4 Target2 : SV_Target2;	// Specular
	float4 Target3 : SV_Target3;	// DiffuseAO
};

PixelInput VS_BasePass(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	float4 WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	Out.Position = mul(WorldPos, ViewProjMatrix); 

	Out.N = mul(In.Normal, (float3x3)InverseTransposeModelMatrix);
	Out.T = mul(In.Tangent.xyz, (float3x3)InverseTransposeModelMatrix);
	Out.B = cross(In.Normal, In.Tangent.xyz) * In.Tangent.w;
	Out.B = mul(Out.B, (float3x3)InverseTransposeModelMatrix);
	
	return Out;
}

void PS_BasePass(PixelInput In, out PixelOutput Out)
{
	float4 Albedo = BaseMap.Sample(LinearSampler, In.Tex);
	float Opacity = OpacityMap.Sample(LinearSampler, In.Tex).r;
	clip(Opacity < 0.1f ? -1 : 1);

	float Metallic = MetallicMap.Sample(LinearSampler, In.Tex).x;
	float AO = AOMap.Sample(LinearSampler, In.Tex).x;
	float3 Emissive = EmissiveMap.Sample(LinearSampler, In.Tex).xyz;

	float3 N = normalize(In.N);

	Out.Target0 = float4(Emissive, 1.0);
	Out.Target1 = float4(0.5 * N + 0.5, 1.0);
	Out.Target2 = float4(Metallic, 0.5, 0, 1.0);
	Out.Target3 = float4(Albedo.rgb, AO);
}