
Texture2D			SceneDiffuse		: register(t0);
Texture2D			SceneSpecular		: register(t1);
Texture2D			SSSBlurColor		: register(t2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

cbuffer PSContant : register(b0)
{
	float3	SubsurfaceColor;
	float   EffectStr;
	int		DebugFlag;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

PixelOutput PS_SSSCombine(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	float3 Diffuse = SceneDiffuse.SampleLevel(LinearSampler, Tex, 0).rgb;
	float3 Specular = SceneSpecular.SampleLevel(LinearSampler, Tex, 0).rgb;
	float3 SSSColor = SSSBlurColor.SampleLevel(LinearSampler, Tex, 0).rgb;

	float3 Color = 0;
	if (DebugFlag == 1)
	{
		Color = Diffuse * SubsurfaceColor + SSSColor * SubsurfaceColor * EffectStr;
	}
	else if (DebugFlag == 2)
	{
		Color = Specular;
	}
	else
	{
		Color += Diffuse * SubsurfaceColor + SSSColor * SubsurfaceColor * EffectStr;
		Color += Specular;
	}

	Out.Target0 = float4(Color, 1);
	return Out;
}


 