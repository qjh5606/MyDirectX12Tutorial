#pragma once
#include "AmbientOcclusion.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "Camera.h"
#include "UserMarkers.h"
#include "Texture.h"

#include <iostream>
#include <ctime>

using namespace std;
using namespace BufferManager;

namespace AmbientOcclusion
{
	bool					g_EnableAO = true;
	int						g_AOType = AOType::GTAO;

	// SSAO
	float					g_SSAOTraceRadius = 0.5f;
	int						g_SSAOBlurRadius = 3;
	std::vector<Vector4f>	Samples;
	
	// HBAO
	int						g_HBAONumDirections = 6;
	int						g_HBAONumSamples = 3;
	float					g_HBAOTraceRadius = 0.5f;
	int						g_HBAOMaxRadiusPixels = 50;
	int						g_HBAOBlurRadius = 2;

	// GTAO
	int						CurrSampleFrame = 0;

	FColorBuffer			g_AOBuffer;
	FTexture				g_SSAONoise;
	FTexture				g_HBAONoise;

	// Shaders and PSOs
	ComPtr<ID3DBlob>		m_ScreenQuadVS;

	FRootSignature			m_AOSignature;
	FGraphicsPipelineState	m_SSAOPSO;
	ComPtr<ID3DBlob>		m_SSAOPS;

	FGraphicsPipelineState	m_SSAOBlurPSO;
	ComPtr<ID3DBlob>		m_SSAOBlurPS;

	FGraphicsPipelineState	m_HBAOPSO;
	ComPtr<ID3DBlob>		m_HBAOPS;

	FGraphicsPipelineState	m_GTAOPSO;
	ComPtr<ID3DBlob>		m_GTAOPS;
}

float RandomFloat(float LO = 0.0f, float HI = 1.0f)
{
	float random = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));
	return random;
}

// make it closer to the origin point
float Lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

void AmbientOcclusion::Initialize()
{
	// SSAO Random rotation vector
	std::vector<Vector4f> SSAONoises;
	for (int i = 0; i < 16; ++i)
	{
		// z is 0
		Vector3f Noise = Vector3f(
			RandomFloat() * 2 - 1.0f,
			RandomFloat() * 2 - 1.0f,
			0);

		Noise = Noise.Normalize();
		SSAONoises.push_back(Vector4f(Noise, 0));
	}
	g_SSAONoise.Create(4, 4, DXGI_FORMAT_R32G32B32A32_FLOAT, &SSAONoises[0]);

	// SSAO Samples
	Samples.resize(64);
	for (int i = 0; i < 64; ++i)
	{
		Vector3f Sample = Vector3f(
			RandomFloat() * 2 - 1.0f,
			RandomFloat() * 2 - 1.0f,
			RandomFloat()
		);
		Sample = Sample.Normalize();
		// Scale
		float Scale = RandomFloat();
		//Scale = 1;
		Scale *= i * 1.0f / 64.0f;
		Scale = Lerp(0.1f, 1.0f, Scale * Scale);
		Sample = Sample * Scale;
		Samples[i] = Vector4f(Sample, 0);
	}

	// HBAO Noise
	std::vector<Vector4f> HBAONoises;
	for (int i = 0; i < 16; ++i)
	{
		float theta = RandomFloat(-MATH_PI / 12, MATH_PI / 12);

		Vector3f Noise = Vector3f(
			std::cos(theta),
			std::sin(theta),
			RandomFloat());

		Noise = Noise.Normalize();
		HBAONoises.push_back(Vector4f(Noise, 0));
	}
	g_HBAONoise.Create(4, 4, DXGI_FORMAT_R32G32B32A32_FLOAT, &HBAONoises[0]);

	//////////////////////////////////////////////////////////////////////////

	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	g_AOBuffer.Create(L"AO Buffer", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());

	// Shaders
	m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
	m_SSAOPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SSAO.hlsl", "PS_SSAO", "ps_5_1");
	m_SSAOBlurPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SSAO_Blur.hlsl", "PS_SSAO_Blur", "ps_5_1");
	m_HBAOPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/HBAO.hlsl", "PS_HBAO", "ps_5_1");
	m_GTAOPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/GTAO.hlsl", "PS_GTAO", "ps_5_1");

	// PSO
	FSamplerDesc LinearSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	FSamplerDesc PointSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	m_AOSignature.Reset(2, 2);
	m_AOSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_AOSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_AOSignature.InitStaticSampler(0, LinearSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	m_AOSignature.InitStaticSampler(1, PointSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	m_AOSignature.Finalize(L"AO");

	// SSAO
	m_SSAOPSO.SetRootSignature(m_AOSignature);
	m_SSAOPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
	m_SSAOPSO.SetBlendState(FPipelineState::BlendDisable);
	m_SSAOPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
	m_SSAOPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_SSAOPSO.SetRenderTargetFormats(1, &g_AOBuffer.GetFormat(), g_SceneDepthZ.GetFormat());
	m_SSAOPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
	m_SSAOPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SSAOPS.Get()));
	m_SSAOPSO.Finalize();

	// SSAO Blur
	m_SSAOBlurPSO = m_SSAOPSO;
	m_SSAOBlurPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SSAOBlurPS.Get()));
	m_SSAOBlurPSO.Finalize();

	// HBAO
	m_HBAOPSO = m_SSAOPSO;
	m_HBAOPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_HBAOPS.Get()));
	m_HBAOPSO.Finalize();

	// GTAO
	m_GTAOPSO = m_SSAOPSO;
	m_GTAOPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_GTAOPS.Get()));
	m_GTAOPSO.Finalize();
}

void AmbientOcclusion::Destroy()
{
	g_AOBuffer.Destroy();
}

void AmbientOcclusion::Render(FCommandContext& CommandContext, FCamera& Camera, AOType Type)
{
	if (g_EnableAO == false)
		return;

	if (Type == AOType::SSAO)
	{
		AmbientOcclusion::RenderSSAO(CommandContext, Camera);
	}
	else if (Type == AOType::HBAO)
	{
		AmbientOcclusion::RenderHBAO(CommandContext, Camera);
	}
	else if (Type == AOType::GTAO)
	{
		AmbientOcclusion::RenderGTAO(CommandContext, Camera);
	}
}

void AmbientOcclusion::RenderSSAO(FCommandContext& CommandContext, FCamera& Camera)
{
	// SSAO Generate
	{
		UserMarker GPUMaker(CommandContext, "SSAO Generate");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_SSAOPSO);

		CommandContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
		CommandContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SSAONoise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_AOBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_AOBuffer);

		__declspec(align(16)) struct
		{
			FMatrix		InvViewProj;
			FMatrix		ViewProj;
			Vector4f	Samples[64];
			float		Radius;
		} Constants;

		Constants.ViewProj = Camera.GetViewMatrix() * Camera.GetProjectionMatrix();
		Constants.InvViewProj = Constants.ViewProj.Inverse();

		for (int i = 0; i < 64; ++i)
		{
			Constants.Samples[i] = Samples[i];
		}

		Constants.Radius = g_SSAOTraceRadius;

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_GBufferA.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 1, g_GBufferC.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 2, g_SceneDepthZ.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 3, g_SSAONoise.GetSRV());

		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}

	// SSAO Blur
	{
		UserMarker GPUMaker(CommandContext, "SSAO Blur");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_SSAOBlurPSO);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_SceneColorBuffer);

		__declspec(align(16)) struct
		{
			Vector2f	texelSize;
			int			BlurRadius;
		} Constants;

		Constants.texelSize = Vector2f(1.0f / static_cast<float>(g_SceneColorBuffer.GetWidth()), 1.0f / static_cast<float>(g_SceneColorBuffer.GetHeight()));
		Constants.BlurRadius = g_SSAOBlurRadius;

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_AOBuffer.GetSRV());
		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}
}

void AmbientOcclusion::RenderHBAO(FCommandContext& CommandContext, FCamera& Camera)
{
	// HBAO Generate
	{
		UserMarker GPUMaker(CommandContext, "HBAO Generate");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_HBAOPSO);

		CommandContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
		CommandContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_HBAONoise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_AOBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_AOBuffer);

		__declspec(align(16)) struct
		{
			FMatrix		InvProjMatrix;
			Vector4f	Resolution;
			float		NumDirections;
			float		NumSamples;
			float		TraceRadius;
			float		MaxRadiusPixels;
			Vector2f	ClipInfo;
		} Constants;

		Constants.InvProjMatrix = Camera.GetProjectionMatrix().Inverse();

		float Width = static_cast<float>(g_AOBuffer.GetWidth());
		float Height = static_cast<float>(g_AOBuffer.GetHeight());
		Constants.Resolution = Vector4f(Width, Height, 1.0f / Width, 1.0f / Height);

		Constants.NumDirections = g_HBAONumDirections;
		Constants.NumSamples = g_HBAONumSamples;
		Constants.TraceRadius = g_HBAOTraceRadius;
		Constants.MaxRadiusPixels = g_HBAOMaxRadiusPixels;

		float fovRad = Camera.GetFovY();
		Constants.ClipInfo[0] = 1.0f / tanf(fovRad * 0.5f) * (Height / Width);
		Constants.ClipInfo[1] = 1.0f / tanf(fovRad * 0.5f);

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_GBufferA.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 1, g_GBufferC.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 2, g_SceneDepthZ.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 3, g_HBAONoise.GetSRV());

		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}

	// HBAO Blur
	{
		UserMarker GPUMaker(CommandContext, "HBAO Blur");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_SSAOBlurPSO);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_SceneColorBuffer);

		__declspec(align(16)) struct
		{
			Vector2f	texelSize;
			int			BlurRadius;
		} Constants;

		Constants.texelSize = Vector2f(1.0f / static_cast<float>(g_SceneColorBuffer.GetWidth()), 1.0f / static_cast<float>(g_SceneColorBuffer.GetHeight()));
		Constants.BlurRadius = g_HBAOBlurRadius;

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_AOBuffer.GetSRV());
		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}
}

void AmbientOcclusion::RenderGTAO(FCommandContext& CommandContext, FCamera& Camera)
{
	const float rotations[6] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
	const float offsets[4] = { 0.1f, 0.6f, 0.35f, 0.85f };

	// GTAO Generate
	{
		UserMarker GPUMaker(CommandContext, "GTAO Generate");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_GTAOPSO);

		CommandContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
		CommandContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_HBAONoise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_AOBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_AOBuffer);

		__declspec(align(16)) struct
		{
			FMatrix		InvProjMatrix;
			FMatrix		ViewMatrix;
			Vector4f	Resolution;			// width height 1/width 1/height
			Vector4f	ClipInfo;
			Vector2f	Params;				// angle and offset
		} Constants;

		Constants.InvProjMatrix = Camera.GetProjectionMatrix().Inverse();
		Constants.ViewMatrix = Camera.GetViewMatrix();

		float Width = static_cast<float>(g_AOBuffer.GetWidth());
		float Height = static_cast<float>(g_AOBuffer.GetHeight());
		Constants.Resolution = Vector4f(Width, Height, 1.0f / Width, 1.0f / Height);

		// start rotation
		Constants.Params[0] = rotations[CurrSampleFrame % 6] / 360.0f;
		Constants.Params[1] = offsets[(CurrSampleFrame / 6) % 4];

		// near far
		Constants.ClipInfo[0] = Camera.GetNearClip();
		Constants.ClipInfo[1] = Camera.GetFarClip();

		float HalfFovY = Camera.GetFovY() * 0.5f;
		float Ratio = Width / Height;
		Constants.ClipInfo[2] = tan(HalfFovY) * Ratio;
		Constants.ClipInfo[3] = tan(HalfFovY);

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_GBufferA.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 1, g_GBufferC.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 2, g_SceneDepthZ.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 3, g_HBAONoise.GetSRV());

		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}

	// GTAO Blur
	{
		UserMarker GPUMaker(CommandContext, "GTAO Blur");

		CommandContext.SetRootSignature(m_AOSignature);
		CommandContext.SetPipelineState(m_SSAOBlurPSO);

		CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
		CommandContext.ClearColor(g_SceneColorBuffer);

		__declspec(align(16)) struct
		{
			Vector2f	texelSize;
			int			BlurRadius;
		} Constants;

		Constants.texelSize = Vector2f(1.0f / static_cast<float>(g_SceneColorBuffer.GetWidth()), 1.0f / static_cast<float>(g_SceneColorBuffer.GetHeight()));
		Constants.BlurRadius = g_HBAOBlurRadius;

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

		CommandContext.SetDynamicDescriptor(1, 0, g_AOBuffer.GetSRV());
		// no need to set vertex buffer and index buffer
		CommandContext.Draw(3);
	}

	CurrSampleFrame = (CurrSampleFrame + 1) % 6;
}
