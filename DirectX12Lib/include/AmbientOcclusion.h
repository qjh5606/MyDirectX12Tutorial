#pragma once

class FCamera;
class FCubeBuffer;
class FColorBuffer;
class FCommandContext;

namespace AmbientOcclusion
{
	enum AOType
	{
		SSAO,
		HBAO,
		GTAO
	};

	extern bool     g_EnableAO;
	extern int		g_AOType;

	// SSAO
	extern float	g_SSAOTraceRadius;
	extern int		g_SSAOBlurRadius;

	// HBAO
	extern int		g_HBAONumDirections;
	extern int		g_HBAONumSamples;
	extern float	g_HBAOTraceRadius;
	extern int		g_HBAOMaxRadiusPixels;
	extern int		g_HBAOBlurRadius;

	void RenderSSAO(FCommandContext& CommandContext, FCamera& Camera);
	void RenderHBAO(FCommandContext& CommandContext, FCamera& Camera);
	void RenderGTAO(FCommandContext& CommandContext, FCamera& Camera);

	void Initialize();
	void Destroy();
	void Render(FCommandContext& CommandContext, FCamera& Camera, AmbientOcclusion::AOType Type);


}