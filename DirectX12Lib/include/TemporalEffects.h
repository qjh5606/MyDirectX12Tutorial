﻿#pragma once
#include <stdint.h>
#include "ColorBuffer.h"

class FCommandContext;
class FComputeContext;

namespace TemporalEffects
{
	extern bool	g_EnableTAA;

	void Initialize(void);

	void Destroy(void);

	// Call once per frame to increment the internal frame counter and, in the case of TAA, choosing the next
	// jittered sample position.
	void Update(void);

	// Returns whether the frame is odd or even
	uint32_t GetFrameIndexMod2(void);

	// Jitter values are neutral at 0.5 and vary from [0, 1).  Jittering only occurs when temporal antialiasing
	// is enabled.  You can use these values to jitter your viewport or projection matrix.
	void GetJitterOffset(float& JitterX, float& JitterY);

	void ClearHistory(FCommandContext& Context);

	void ResolveImage(FCommandContext& GraphicsContext, FColorBuffer& SceneColor);

	void ApplyTemporalAA(FComputeContext& Context, FColorBuffer& SceneColor);
	void SharpenImage(FComputeContext& Context, FColorBuffer& SceneColor, FColorBuffer& TemporalColor);

}