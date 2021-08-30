#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"
#include "RenderWindow.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "DirectXTex.h"
#include "SamplerManager.h"
#include "Model.h"
#include "ShadowBuffer.h"
#include "Light.h"
#include "CubeBuffer.h"
#include "GameInput.h"
#include "ImguiManager.h"
#include "GenerateMips.h"
#include "TemporalEffects.h"
#include "BufferManager.h"
#include "MotionBlur.h"
#include "PostProcessing.h"
#include "DepthOfField.h"
#include "UserMarkers.h"
#include "AmbientOcclusion.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include<sstream>

extern FCommandListManager g_CommandListManager;

const int PREFILTERED_SIZE = 256;

using namespace BufferManager;

class TutorialAO : public FGame
{
public:
	TutorialAO(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupShaders();
		SetupMesh();
		SetupPipelineState();
		SetupCameraLight();

		PostProcessing::g_EnableBloom = false;
		PostProcessing::g_EnableSSR = false;
		PostProcessing::g_EnableTonmapping = false;
		TemporalEffects::g_EnableTAA = false;
	}

	void OnShutdown()
	{
	}

	void OnUpdate()
	{
		tEnd = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		tStart = std::chrono::high_resolution_clock::now();
		m_Camera.Update(delta);
		
		m_Sponza->Update();
		if (m_RotateMesh)
		{
			m_RotateY += delta * 0.0005f;
			m_RotateY = fmodf(m_RotateY, MATH_2PI);
		}
		m_Sponza->SetRotation(FMatrix::RotateY(m_RotateY));

		if (GameInput::IsKeyDown('F'))
			SetupCameraLight();

		if (GameInput::IsKeyDown(32))
		{
			if (m_RotateMesh)
			{
				m_RotateMesh = false;
			}
			else
			{
				m_RotateMesh = true;
			}
		}

		m_MainViewport.TopLeftX = 0.0f;
		m_MainViewport.TopLeftY = 0.0f;

		m_MainViewport.Width = (float)RenderWindow::Get().GetBackBuffer().GetWidth();
		m_MainViewport.Height = (float)RenderWindow::Get().GetBackBuffer().GetHeight();
		m_MainViewport.MinDepth = 0.0f;
		m_MainViewport.MaxDepth = 1.0f;

		m_MainScissor.left = 0;
		m_MainScissor.top = 0;
		m_MainScissor.right = (LONG)RenderWindow::Get().GetBackBuffer().GetWidth();
		m_MainScissor.bottom = (LONG)RenderWindow::Get().GetBackBuffer().GetHeight();

		TemporalEffects::Update();
	}

	void OnGUI(FCommandContext& CommandContext)
	{
		static bool ShowConfig = true;
		if (!ShowConfig)
			return;

		ImguiManager::Get().NewFrame();

		ImGui::SetNextWindowPos(ImVec2(1, 1));
		ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
		if (ImGui::Begin("Config", &ShowConfig, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Separator();

			ImGui::BeginGroup();
			ImGui::Text("Show Mode");
			ImGui::Indent(20);
			ImGui::RadioButton("SSAO", &AmbientOcclusion::g_AOType, AmbientOcclusion::AOType::SSAO);
			ImGui::RadioButton("HBAO", &AmbientOcclusion::g_AOType, AmbientOcclusion::AOType::HBAO);
			ImGui::RadioButton("GTAO", &AmbientOcclusion::g_AOType, AmbientOcclusion::AOType::GTAO);
			ImGui::Indent(-20);
			ImGui::EndGroup();

			if (AmbientOcclusion::g_AOType == AmbientOcclusion::AOType::SSAO)
			{
				ImGui::SliderFloat("SSAO Trace Radius", &AmbientOcclusion::g_SSAOTraceRadius, 0.1f, 1.0f);
				ImGui::SliderInt("SSAO Blur Radius", &AmbientOcclusion::g_SSAOBlurRadius, 1, 5);
			}
			else if (AmbientOcclusion::g_AOType == AmbientOcclusion::AOType::HBAO)
			{
				ImGui::SliderInt("HBAO Num Directions", &AmbientOcclusion::g_HBAONumDirections, 4, 8);
				ImGui::SliderInt("HBAO Num Samples", &AmbientOcclusion::g_HBAONumSamples, 3, 5);
				ImGui::SliderFloat("HBAO Trace Radius", &AmbientOcclusion::g_HBAOTraceRadius, 0.1f, 1.0f);
				ImGui::SliderInt("HBAO Max RadiusPixels", &AmbientOcclusion::g_HBAOMaxRadiusPixels, 30, 50);
				ImGui::SliderInt("HBAO Blur Radius", &AmbientOcclusion::g_HBAOBlurRadius, 1, 5);
			}
			else if (AmbientOcclusion::g_AOType == AmbientOcclusion::AOType::GTAO)
			{

			}

		}
		ImGui::End();

		ImguiManager::Get().Render(CommandContext, RenderWindow::Get());
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		BasePass(CommandContext, true);
		
		AmbientOcclusion::Render(CommandContext, m_Camera,(AmbientOcclusion::AOType)AmbientOcclusion::g_AOType);

		PostProcessing::Render(CommandContext);

		OnGUI(CommandContext);

		CommandContext.TransitionResource(RenderWindow::Get().GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}

private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.0f, 0, -7.f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_Camera.SetMouseMoveSpeed(1e-3f);
		m_Camera.SetMouseRotateSpeed(1e-4f);

		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}

	void ParsePath()
	{
		size_t lastPeriodIndex = m_HDRFilePath.find_last_of('.');

		if (lastPeriodIndex == m_HDRFilePath.npos)
		{
			printf("Input HDR file pathv is wrong\n");
			exit(-1);
		}

		m_HDRFileName = m_HDRFilePath.substr(0, lastPeriodIndex);

		m_IrradianceMapPath = m_HDRFileName + std::wstring(L"_IrradianceMap.dds");
		m_PrefilteredMapPath = m_HDRFileName + std::wstring(L"_PrefilteredMap.dds");
		m_SHCoeffsPath = m_HDRFileName + std::wstring(L"_SHCoeffs.txt");
		m_PreIntegrateBRDFPath = std::wstring(L"../Resources/HDR/PreIntegrateBRDF.dds");

		// check file exit
		bool isExist = true;
		isExist &= CheckFileExist(m_IrradianceMapPath);
		isExist &= CheckFileExist(m_PrefilteredMapPath);
		isExist &= CheckFileExist(m_SHCoeffsPath);
		isExist &= CheckFileExist(m_PreIntegrateBRDFPath);
		if (!isExist)
		{
			printf("this input HDR file do not precompute to generate IBL maps\n");
			exit(-1);
		}
	}

	bool CheckFileExist(const std::wstring& name)
	{
		std::ifstream f(name.c_str());
		const bool isExist = f.good();
		f.close();
		return isExist;
	}

	void SetupMesh()
	{
		m_Sponza = std::make_unique<FModel>("../Resources/Models/sponza/sponza.obj");
		m_Sponza->SetScale(0.01f, 0.01f, 0.01f);
		m_Sponza->SetPosition(0, -2.0f, 0);

		m_Lucky = std::make_unique<FModel>("../Resources/Models/Lucy/Lucy.obj");
		m_Lucky->SetScale(0.015f, 0.015f, 0.015f);
		m_Lucky->SetPosition(0, -2.0f, 0);
	}

	void SetupShaders()
	{
		m_MeshVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SponzaBasePass.hlsl", "VS_BasePass", "vs_5_1");
		m_MeshPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SponzaBasePass.hlsl", "PS_BasePass", "ps_5_1");
	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		// Mesh
		m_MeshSignature.Reset(3, 1);
		m_MeshSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_MeshSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_MeshSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10); //pbr 7, irradiance, prefiltered, preintegratedGF
		m_MeshSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_MeshSignature.Finalize(L"Mesh RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_Sponza->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_MeshPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_MeshPSO.SetRootSignature(m_MeshSignature);

		D3D12_RASTERIZER_DESC CullBack = FPipelineState::RasterizerTwoSided;
		CullBack.DepthClipEnable = TRUE;
		CullBack.CullMode = D3D12_CULL_MODE_BACK;
		m_MeshPSO.SetRasterizerState(CullBack);
		//m_MeshPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_MeshPSO.SetBlendState(FPipelineState::BlendDisable);

		m_MeshPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);

		m_MeshPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		DXGI_FORMAT RTFormats[] = {
			g_SceneColorBuffer.GetFormat(), g_GBufferA.GetFormat(), g_GBufferB.GetFormat(), g_GBufferC.GetFormat(), MotionBlur::g_VelocityBuffer.GetFormat(),
		};
		m_MeshPSO.SetRenderTargetFormats(5, RTFormats, g_SceneDepthZ.GetFormat());
		m_MeshPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_MeshVS.Get()));
		m_MeshPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_MeshPS.Get()));
		m_MeshPSO.Finalize();

	}

	void BasePass(FCommandContext& GfxContext, bool Clear)
	{
		UserMarker GPUMaker(GfxContext, "Base Pass");

		// Set necessary state.
		GfxContext.SetRootSignature(m_MeshSignature);
		GfxContext.SetPipelineState(m_MeshPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(SceneBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferB, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(MotionBlur::g_VelocityBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] = {
			SceneBuffer.GetRTV(), g_GBufferA.GetRTV(), g_GBufferB.GetRTV(), g_GBufferC.GetRTV(), MotionBlur::g_VelocityBuffer.GetRTV(),
		};
		GfxContext.SetRenderTargets(5, RTVs, g_SceneDepthZ.GetDSV());

		if (Clear)
		{
			GfxContext.ClearColor(SceneBuffer);
			GfxContext.ClearColor(g_GBufferA);
			GfxContext.ClearColor(g_GBufferB);
			GfxContext.ClearColor(g_GBufferC);
			GfxContext.ClearColor(MotionBlur::g_VelocityBuffer);
			GfxContext.ClearDepth(DepthBuffer);
		}

		__declspec(align(16)) struct
		{
			FMatrix ModelMatrix;
			FMatrix ViewProjMatrix;
			FMatrix InverseTransposeModelMatrix;
		} BasePass_VSConstants;

		BasePass_VSConstants.ModelMatrix = m_Sponza->GetModelMatrix();
		BasePass_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		BasePass_VSConstants.InverseTransposeModelMatrix = m_Sponza->GetModelMatrix().Inverse().Transpose();

		GfxContext.SetDynamicConstantBufferView(0, sizeof(BasePass_VSConstants), &BasePass_VSConstants);

		__declspec(align(16)) struct
		{
			float		Exposure;
			Vector3f	CameraPos;
			FMatrix		InvViewProj;
		} BasePass_PSConstants;

		BasePass_PSConstants.CameraPos = m_Camera.GetPosition();
		GfxContext.SetDynamicConstantBufferView(1, sizeof(BasePass_PSConstants), &BasePass_PSConstants);

		m_Sponza->Draw(GfxContext);

#if 0
		BasePass_VSConstants.ModelMatrix = m_Lucky->GetModelMatrix();
		GfxContext.SetDynamicConstantBufferView(0, sizeof(BasePass_VSConstants), &BasePass_VSConstants);
		m_Lucky->Draw(GfxContext);
#endif
	}

private:
	std::wstring m_HDRFilePath;
	std::wstring m_HDRFileName;
	std::wstring m_IrradianceMapPath;
	std::wstring m_PrefilteredMapPath;
	std::wstring m_SHCoeffsPath;
	std::wstring m_PreIntegrateBRDFPath;

private:
	ComPtr<ID3DBlob>			m_MeshVS;
	ComPtr<ID3DBlob>			m_MeshPS;
	FRootSignature				m_MeshSignature;
	FGraphicsPipelineState		m_MeshPSO;

	int							m_AOType = AmbientOcclusion::AOType::SSAO;

	// 
	D3D12_VIEWPORT		m_MainViewport;
	D3D12_RECT			m_MainScissor;
	FCamera m_Camera;

	std::unique_ptr<FModel> m_Sponza;
	std::unique_ptr<FModel> m_Lucky;

	float m_RotateY = MATH_PI / 2;
	bool m_RotateMesh = false;

	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Ambient Occlusion";
	TutorialAO tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}