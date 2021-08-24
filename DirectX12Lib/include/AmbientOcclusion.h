#pragma once

class FCamera;
class FCubeBuffer;
class FColorBuffer;
class FCommandContext;

namespace AmbientOcclusion
{
	extern bool     g_EnableAO;
	extern float	g_TraceRadius;
	extern float	g_Bias;
	extern int		g_BlurRadius;

	void Initialize();
	void Destroy();
	void Render(FCommandContext& CommandContext, FCamera& Camera);
}