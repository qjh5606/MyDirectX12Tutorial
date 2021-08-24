#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D		AOBuffer		: register(t0);

SamplerState LinearSampler		: register(s0);
SamplerState WrapLinearSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float2	texelSize;
	int		BlurRadius;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

PixelOutput PS_SSAO_Blur(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	float result = 0;

	for (int x = -BlurRadius; x < BlurRadius; ++x)
	{
		for (int y = -BlurRadius; y < BlurRadius; ++y)
		{
			float2 offset = Tex + float2(x, y) * texelSize;
			result += AOBuffer.SampleLevel(LinearSampler, offset, 0).r;
		}
	}

	result /= (float)(BlurRadius * BlurRadius * 4);

	Out.Target0 = float4(result, result, result, 1);
	return Out;
}


