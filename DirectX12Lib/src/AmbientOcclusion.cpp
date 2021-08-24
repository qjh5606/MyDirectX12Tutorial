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

	//SSAO
	float					g_TraceRadius = 0.5;
	float					g_Bias = 0.025f;
	int						g_BlurRadius = 3;

	std::vector<Vector4f>	Samples;

	FColorBuffer			g_AOBuffer;
	FTexture				g_Noise;

	// Shaders and PSOs
	ComPtr<ID3DBlob>		m_ScreenQuadVS;

	FRootSignature			m_AOSignature;
	FGraphicsPipelineState	m_SSAOPSO;
	ComPtr<ID3DBlob>		m_SSAOPS;

	FGraphicsPipelineState	m_SSAOBlurPSO;
	ComPtr<ID3DBlob>		m_SSAOBlurPS;
}

float RandomFloat()
{
	float random = ((float)rand()) / (float)RAND_MAX;
	return random;
}

// make it closer to the origin point
float Lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

void AmbientOcclusion::Initialize()
{
	std::vector<Vector4f> Noises;

	// Random rotation vector
	for (int i = 0; i < 16; ++i)
	{
		// z is 0

		Vector3f Noise = Vector3f(
			RandomFloat() * 2 - 1.0f,
			RandomFloat() * 2 - 1.0f,
			0);

		Noise = Noise.Normalize();
		Noises.push_back(Vector4f(Noise, 0));
	}

	g_Noise.Create(4, 4, DXGI_FORMAT_R32G32B32A32_FLOAT, &Noises[0]);

	// Samples
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

	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	// Buffers
	g_AOBuffer.Create(L"AO Buffer", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());

	// Shaders and PSOs 
	m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");

	m_SSAOPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SSAO.hlsl", "PS_SSAO", "ps_5_1");

	m_SSAOBlurPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SSAO_Blur.hlsl", "PS_SSAO_Blur", "ps_5_1");

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
	m_SSAOBlurPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SSAOBlurPS.Get()));;
	m_SSAOBlurPSO.Finalize();
}

void AmbientOcclusion::Destroy()
{
	g_AOBuffer.Destroy();
}

void AmbientOcclusion::Render(FCommandContext& CommandContext, FCamera& Camera)
{
	if (g_EnableAO == false)
		return;

	{
		// SSAO Generate
		{
			UserMarker GPUMaker(CommandContext, "SSAO Generate");

			CommandContext.SetRootSignature(m_AOSignature);
			CommandContext.SetPipelineState(m_SSAOPSO);

			CommandContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
			CommandContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(g_Noise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			CommandContext.TransitionResource(g_AOBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			CommandContext.SetRenderTargets(1, &g_AOBuffer.GetRTV(), g_SceneDepthZ.GetDSV());
			CommandContext.ClearColor(g_AOBuffer);

			__declspec(align(16)) struct
			{
				FMatrix		InvViewProj;
				FMatrix		ViewProj;
				Vector4f	Samples[64];
				float		Radius;
				float		Bias;
				float		Near;
				float		Far;
			} Constants;

			Constants.ViewProj = Camera.GetViewMatrix() * Camera.GetProjectionMatrix();
			Constants.InvViewProj = Constants.ViewProj.Inverse();

			for (int i = 0; i < 64; ++i)
			{
				Constants.Samples[i] = Samples[i];
			}

			Constants.Radius = g_TraceRadius;
			Constants.Bias = g_Bias;

			Constants.Near = Camera.GetNearClip();
			Constants.Far = Camera.GetFarClip();

			CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

			CommandContext.SetDynamicDescriptor(1, 0, g_GBufferA.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 1, g_GBufferC.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 2, g_SceneDepthZ.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 3, g_Noise.GetSRV());

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
			Constants.BlurRadius = g_BlurRadius;

			CommandContext.SetDynamicConstantBufferView(0, sizeof(Constants), &Constants);

			CommandContext.SetDynamicDescriptor(1, 0, g_AOBuffer.GetSRV());
			// no need to set vertex buffer and index buffer
			CommandContext.Draw(3);
		}


	}




}
