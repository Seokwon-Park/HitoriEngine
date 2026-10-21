#include "EnginePCH.h"
#include "Render/SceneRenderer.h"
#include "Render/Renderer.h"
#include "Render/SceneLightResources.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/SceneViewTargets.h"
#include "Render/SkyboxRenderer.h"
#include "Render/RenderResourceManager.h"
#include "Engine/World.h"
#include "Tasks/Tasks.h"
#include "Core/Stats/LightweightStats.h"
#include "Components/StaticMeshComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Render/SkyLightEnvironment.h"
#include "Components/LocalLightComponent.h"

#include <cmath>
#include <cstring>
#include <utility>

DECLARE_CYCLE_STAT("Frustum Cull", STAT_FrustumCull);
DECLARE_CYCLE_STAT("Gather Elements", STAT_GatherElements);

namespace
{
	FRenderGraphRasterPassDesc MakeViewportPass(const std::string& Name, FRenderGraphTextureRef Color,
		FRenderGraphTextureRef DepthTarget = {},
		ERenderGraphLoadAction DepthLoad = ERenderGraphLoadAction::Load)
	{
		FRenderGraphRasterPassDesc Pass{};
		Pass.Name = Name;
		Pass.ColorAttachments = { { Color, ERenderGraphLoadAction::Load } };
		if (DepthTarget) Pass.DepthAttachment = { DepthTarget, DepthLoad, 1.0f, 0 };
		return Pass;
	}

	bool AddOverlayPass(FRenderGraphBuilder& Graph, const char* Name,
		FRenderGraphTextureRef Color, FRenderGraphTextureRef Depth,
		const std::function<void()>& Draw,
		ERenderGraphLoadAction DepthLoad = ERenderGraphLoadAction::Load)
	{
		return !Draw || Graph.AddRasterPass(MakeViewportPass(Name, Color, Depth, DepthLoad), Draw);
	}
}

// 생성자
FSceneRenderer::FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData)
	: World(InWorld), View(InView), RenderData(InRenderData)
{
	CachedViewContext = View.ToViewContext();
}

// 구체와 절두체 평면 교차 판별
bool FSceneRenderer::IntersectSphereFrustum(const FVector4& PositionRadius, const FPlane(&Planes)[6]) const
{
	const float Radius = PositionRadius.W;
	for (const FPlane& Plane : Planes)
	{
		const float Distance = Plane.Normal.X * PositionRadius.X +
			Plane.Normal.Y * PositionRadius.Y +
			Plane.Normal.Z * PositionRadius.Z + Plane.Distance;

		if (Distance < -Radius)
		{
			return false;
		}
	}
	return true;
}

void FSceneRenderer::AddInteractingPrimitives(FProjectedShadowInfo* Shadow)
{
	FScene& Scene = World->GetScene();
	const FFrustumPlanes& Frustum = Shadow->CasterFrustum;

	Scene.BVH.QueryCull(FrustumAllPlanesMask, [&Frustum](const FBox& Bounds, uint32& Mask)
		{
			return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, Frustum, Mask));
		},
		[&](FPrimitiveSceneProxy* Proxy)
		{
			if (!Proxy->bCastDynamicShadow) return;
			if (!Proxy->IsShown(View.bGameView)) return;
			if (!Proxy->GetMesh()) return;

			Shadow->AddSubjectPrimitive(Proxy);
		});
}

// 가시성 검사 및 렌더 큐 수집
void FSceneRenderer::InitViews(FRenderer* Renderer)
{
	bLightUploadSucceeded = false;
	if (!World)
	{
		RenderData.ResetFrame();
		return;
	}

	// 월드 프리미티브 수집 및 절두체 컬링
	GatherRenderPackets(World->GetScene(), RenderData, CachedViewContext, &View.Frustum, Renderer);
	RenderData.AmbientLight = World->GetScene().GetSkyAmbientLight(View.bGameView);

	// 렌더 큐 자체 정렬
	RenderData.RenderQueue.Sort();

	// 모든 프록시가 같은 FLightRenderParameters를 채우고, 종류는 Type으로만 구분한다.
	const FScene& Scene = World->GetScene();

	RenderData.VisibleLightInfos.SetNum(Scene.Lights.Num());
	InitDynamicShadows();

	for (const FDirectionalLightSceneProxy* Light : Scene.GetDirectionalLights())
	{
		if (!Light->IsEnabled() || !Light->IsShown(View.bGameView)) continue;
		FLightRenderParameters Parameters{};
		Light->GetLightShaderParameters(Parameters);
		Parameters.ShadowMapChannel = -1;
		const FLightSceneInfo* Info = Light->GetLightSceneInfo();
		if (Info && Info->GetDynamicShadowMapChannel() >= 0)
		{
			FVisibleLightInfo& VisibleInfo = RenderData.VisibleLightInfos[Info->Id];
			if (!VisibleInfo.ShadowsToProject.IsEmpty())
			{
				Parameters.ShadowMapChannel = Info->GetDynamicShadowMapChannel();
			}
		}
		RenderData.VisibleLights.Add(Parameters);
	}

	for (const FLocalLightSceneProxy* Light : Scene.GetLocalLights())
	{
		if (!Light->IsEnabled() || !Light->IsShown(View.bGameView)) continue;
		FLightRenderParameters Parameters{};
		Light->GetLightShaderParameters(Parameters);
		Parameters.ShadowMapChannel = -1;
		const FLightSceneInfo* Info = Light->GetLightSceneInfo();
		if (Info && Info->GetDynamicShadowMapChannel() >= 0)
		{
			FVisibleLightInfo& VisibleInfo = RenderData.VisibleLightInfos[Info->Id];
			if (!VisibleInfo.ShadowsToProject.IsEmpty())
			{
				Parameters.ShadowMapChannel = Info->GetDynamicShadowMapChannel();
			}
		}
		RenderData.VisibleLights.Add(Parameters);
	}

	if (RenderData.ViewLightResources.Init())
		bLightUploadSucceeded = RenderData.ViewLightResources.UploadViewLights(RenderData.VisibleLights, LightCounts, LightOffsets);

	// FLightSceneInfo::Id로 찾는 라이트별 그림자 목록. 그림자는 2단계 InitDynamicShadows에서 채운다.
}

void FSceneRenderer::InitDynamicShadows()
{
	FScene& Scene = World->GetScene();
	for (FLightSceneInfo* Info : Scene.Lights)
	{
		if (Info == nullptr || !Info->Proxy) continue;
		FLightSceneProxy* Proxy = Info->Proxy;
		if (Info->GetDynamicShadowMapChannel() < 0) continue;
		if (!Proxy->CastsDynamicShadow() || !Proxy->IsEnabled() || !Proxy->IsShown(View.bGameView))continue;
		if (!IsLightInView(Proxy)) continue;

		ELightType Type = Proxy->GetLightComponent()->GetLightType();

		// 카메라에 따라 바뀌는 그림자 (Directional CSM)
		if (Proxy->GetNumViewDependentWholeSceneShadows(View) > 0)
		{
			CreateViewDependentWholeSceneShadows(Info);   // 캐스케이드마다 Initializer 받아서 생성
		}
		// 라이트 범위로 정해지는 그림자 (Spot / Point / Rect)
		else
		{
			CreateWholeSceneProjectedShadow(Info);
		}
	}
	AllocateShadowDepthTargets();
}

bool FSceneRenderer::BindDeferredLighting(FRenderer* Renderer, uint32 Width, uint32 Height)
{
	FSceneLightResources& LightResources = RenderData.ViewLightResources;
	if (!bLightUploadSucceeded || !LightResources.GetDeferredConstants()) return false;

	FDeferredLightConstants Constants{};
	Constants.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y,
		View.ViewLocation.Z, View.bIsPerspective ? 1.0f : 0.0f);
	Constants.CameraForward = FVector4(View.ViewForward.X, View.ViewForward.Y, View.ViewForward.Z, 0.0f);

	const FSkyLightEnvironment* Sky = RenderData.bHasSkyLight ? World->GetScene().FindSkyLightEnvironment(RenderData.SkyLight.SourcePath) : nullptr;
	Constants.Ambient = Sky ? FAmbientLightRenderData{ FVector4(0.0f) } : RenderData.AmbientLight; // 지금 환경광 계산이 ambient + skylight로 되어있어서 sky 잇으면 ambient 0으로 해줘야됨

	if (Sky) Constants.SkyColorIntensity = RenderData.SkyLight.ColorIntensity;

	std::memcpy(Constants.LightCounts, LightCounts, sizeof(LightCounts));
	std::memcpy(Constants.LightOffsets, LightOffsets, sizeof(LightOffsets)); // LightOffset은 각 조명별로 시작하는 위치

	Constants.ViewportSize = FVector4(static_cast<float>(Width), static_cast<float>(Height), 0.0f, 0.0f);
	Renderer->UpdateConstant(LightResources.GetDeferredConstants(), &Constants, sizeof(Constants));
	Renderer->BindConstant(0, LightResources.GetDeferredConstants(), EShaderBindFlagBits::Pixel);
	Renderer->BindStructured(3, LightCounts[0] || LightCounts[1] || LightCounts[2] || LightCounts[3] ? LightResources.GetSceneLights() : nullptr, EShaderBindFlagBits::Pixel);
	Renderer->BindCube(4, Sky ? Sky->GetIrradianceCube() : nullptr, EShaderBindFlagBits::Pixel);
	Renderer->BindSampler(1, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	return true;
}

bool FSceneRenderer::BindForwardLighting(FRenderer* Renderer, const FSceneRenderOptions& Options)
{
	FSceneLightResources& LightResources = RenderData.ViewLightResources;
	if (!bLightUploadSucceeded || !LightResources.GetForwardConstants()) return false;

	FPipelineState* BasePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Forward);
	FShaderProgram* Program = FRenderResourceManager::GetForwardShaderProgram(Options.LightingModel);

	if (!BasePSO || !Program) return false;
	FForwardLightingConstants Constants{};

	const FSkyLightEnvironment* Sky = RenderData.bHasSkyLight ? World->GetScene().FindSkyLightEnvironment(RenderData.SkyLight.SourcePath) : nullptr;
	Constants.Ambient = Sky ? FAmbientLightRenderData{ FVector4(0.0f) } : RenderData.AmbientLight;

	if (Sky) Constants.SkyColorIntensity = RenderData.SkyLight.ColorIntensity;
	Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y, View.ViewLocation.Z, 1.0f);

	std::memcpy(Constants.LightCounts, LightCounts, sizeof(LightCounts));
	std::memcpy(Constants.LightOffsets, LightOffsets, sizeof(LightOffsets));

	Renderer->UpdateConstant(LightResources.GetForwardConstants(), &Constants, sizeof(Constants));
	Renderer->BindConstant(1, LightResources.GetForwardConstants(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	Renderer->BindCube(11, Sky ? Sky->GetIrradianceCube() : nullptr, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	Renderer->BindSampler(1, ESamplerState::LinearClamp, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	FForwardLightTileGrid& Tiles = LightResources.GetTiles();
	Tiles.PreparePixelShader(Renderer, bTilesBuilt);
	Renderer->BindPixelConstant(3, Tiles.GetPixelConstants());
	const std::array<ID3D11ShaderResourceView*, 1> TileSRVs = { Tiles.GetPixelResource() };
	Renderer->BindRawPixelResources(9, TileSRVs);

	Renderer->BindStructured(8, (LightCounts[0] || LightCounts[1] || LightCounts[2] || LightCounts[3]) &&
		LightResources.GetSceneLights() && LightResources.GetSceneLights()->GetSRV()
		? LightResources.GetSceneLights() : nullptr,
		EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	return true;
}

void FSceneRenderer::UnbindForwardLighting(FRenderer* Renderer)
{
	Renderer->UnbindResources(9, 1, EShaderBindFlagBits::Pixel);
	Renderer->BindRawPixelConstant(3, nullptr);
	RenderData.ViewLightResources.GetTiles().ResetPixelState();
	Renderer->UnbindResources(8, 1, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	Renderer->UnbindResources(11, 1, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
}

bool FSceneRenderer::AddWorldNormalPass(FRenderGraphBuilder& Graph, FRenderer* Renderer,
	FSceneViewTargets& Targets, const FSceneRenderOptions& Options)
{
	const FRenderGraphTextureRef SceneColor = Graph.ImportTexture(Targets.SceneColor.get(), "SceneColor");
	const FRenderGraphTextureRef Depth = Graph.ImportTexture(Targets.Depth.get(), "SceneDepth");
	if (!SceneColor || !Depth) return false;

	FRenderGraphRasterPassDesc Pass{};
	Pass.Name = "WorldNormal";
	Pass.ColorAttachments = { { SceneColor, ERenderGraphLoadAction::Clear, { 0.0f, 0.0f, 0.0f, 1.0f } } };
	Pass.DepthAttachment = { Depth, ERenderGraphLoadAction::Clear, 1.0f, 0 };
	return Graph.AddRasterPass(std::move(Pass), [this, Renderer, bDrawPrimitives = Options.bDrawPrimitives]
		{
			if (!bDrawPrimitives) return;
			FMeshPassProcessor Processor;
			TArray<FMeshDrawBatch> Batches;
			bCorePassSucceeded = Processor.Build(View, RenderData.RenderQueue,
				&RenderData, EMeshPassType::WorldNormal, Batches);
			if (bCorePassSucceeded)
				Renderer->DrawMeshes(Batches);
		});
}

bool FSceneRenderer::IsLightInView(FLightSceneProxy* Proxy)
{
	ELightType Type = Proxy->GetLightComponent()->GetLightType();

	switch (Type)
	{
	case ELightType::Point:
	case ELightType::Spot:
	case ELightType::Rect:
	{
		const auto* Local = static_cast<const ULocalLightComponent*>(Proxy->GetLightComponent());
		const float R = Local->GetAttenuationRadius();
		return IsAABBInFrustum(FAABB{ Local->GetWorldLocation(), FVector(R, R, R) }, View.Frustum);
	}
	case ELightType::Directional:
		return true;
	case ELightType::Sky:
		break;
	default:
		break;
	}

	return false;
}

bool FSceneRenderer::AddCorePasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
	const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox)
{
	if (!World || !Renderer || !Targets.SceneColor || !Targets.Depth)
		return false;
	const uint32 Width = Targets.SceneColor->GetWidth();
	const uint32 Height = Targets.SceneColor->GetHeight();
	if (!Width || !Height || !Targets.IsValidFor(Width, Height))
		return false;

	bCorePassSucceeded = true;
	bTilesBuilt = false;
	if (Options.bShowWorldNormal)
		return AddWorldNormalPass(Graph, Renderer, Targets, Options);

	FRenderGraphTextureRef ShadowRef{};
	FTexture2D* ShadowMap = nullptr;
	if (!RenderData.ProjectedShadows.IsEmpty())
	{
		ShadowMap = Renderer->GetShadowAtlas(ShadowAtlasSize);
		ShadowRef = Graph.ImportTexture(ShadowMap, "ShadowDepth");

		FRenderGraphRasterPassDesc Pass{};
		Pass.Name = "ShadowDepths";
		Pass.DepthAttachment = { ShadowRef, ERenderGraphLoadAction::Clear, 1.0f, 0 };   // 깊이만, 색 없음
		if (!Graph.AddRasterPass(std::move(Pass), [this, Renderer]
			{
				FPipelineState* DepthPipeline = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_DepthOnly);
				for (const TUniquePtr<FProjectedShadowInfo>& Shadow : RenderData.ProjectedShadows)
				{
					if (!Shadow->bAllocated || Shadow->ShadowDepthBatches.IsEmpty()) continue;

					// 그림자 칸으로 viewport (그래프는 패스 시작 때 텍스처 전체로 맞춰 둠)
					RenderCommand::SetViewport(Shadow->X, Shadow->Y, Shadow->ResolutionX, Shadow->ResolutionY);

					// Depth Prepass와 같은 방식: VS는 Forward 그대로, PS만 깊이 전용
					FShaderProgram DepthProgram(Shadow->ShadowDepthBatches.Front().Pipeline.Shader->VertexShader,
						DepthPipeline->Shader->PixelShader);
					TArray<FMeshDrawBatch> Batches = Shadow->ShadowDepthBatches;
					for (FMeshDrawBatch& Batch : Batches)
					{
						Batch.Pipeline = *DepthPipeline;
						Batch.Pipeline.Shader = &DepthProgram;
						Batch.Pipeline.RasterizerState = ERasterizerState::ShadowDepth;
					}
					Renderer->DrawMeshes(Batches);
				}
			})) return false;
	}

	if (RenderData.bHasSkyLight)
	{
		if (FSkyLightEnvironment* Pending = World->GetScene().FindPendingSkyLightEnvironment(RenderData.SkyLight.SourcePath))
		{
			FRenderGraphComputePassDesc Environment{};
			Environment.Name = "BuildSkyEnvironment";
			Environment.ReadResources = { Pending->GetPanoramaResource() };
			Environment.WriteResources = { Pending->GetEnvironmentResource() };
			if (!Graph.AddComputePass(std::move(Environment), [Pending, Renderer]
				{
					Pending->DispatchEnvironment(Renderer);
				})) return false; // 먼저 SkySphere를 바탕으로 Cubemap 생성, 나중에 물체에 반사광 띄우려면 이거 필요

			FRenderGraphComputePassDesc Irradiance{};
			Irradiance.Name = "BuildSkyIrradiance";
			Irradiance.ReadResources = { Pending->GetEnvironmentResource() };
			Irradiance.WriteResources = { Pending->GetIrradianceResource() };
			if (!Graph.AddComputePass(std::move(Irradiance), [Pending, Renderer]
				{
					Pending->DispatchIrradiance(Renderer);
					Pending->Complete();
				})) return false; // Sky Cubemap 바탕으로 SkyIrradiance cubemap 생성
		}
	}

	const FRenderGraphTextureRef SceneColor = Graph.ImportTexture(Targets.SceneColor.get(), "SceneColor"); // Texture Handle 리턴 해줌
	const FRenderGraphTextureRef Depth = Graph.ImportTexture(Targets.Depth.get(), "SceneDepth");
	if (!SceneColor || !Depth)
		return false;

	const FRenderGraphTextureDesc HDRDesc{ Width, Height, DXGI_FORMAT_R16G16B16A16_FLOAT };
	const FRenderGraphTextureRef LightingHDR = Graph.CreateTexture(HDRDesc, "LightingHDR");
	if (!LightingHDR)
		return false;

	// HDR 파노라마를 물체보다 먼저 그려 두고, 최종 ToneMap에서 함께 인코딩한다.
	FRenderGraphRasterPassDesc Sky{};
	Sky.Name = "Skybox";
	Sky.ColorAttachments = {
		{ LightingHDR, ERenderGraphLoadAction::Clear, { 0.0f, 0.0f, 0.0f, 1.0f } }
	};
	if (!Graph.AddRasterPass(std::move(Sky), [this, Skybox]
		{
			if (bCorePassSucceeded && Skybox)
				Skybox->OnRender(View.ViewProjectionMatrix, View.ViewLocation);
		})) return false;

	if (Options.RenderingPath == ERenderingPath::Deferred) // Deferred rendering
	{
		const FRenderGraphTextureDesc GBufferDesc{ Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM };
		const FRenderGraphTextureRef BaseColor = Graph.CreateTexture(GBufferDesc, "BaseColorMetallic"); // 임시 텍스쳐 생성
		const FRenderGraphTextureRef Normal = Graph.CreateTexture(HDRDesc, "NormalRoughness");
		if (!BaseColor || !Normal)
			return false;

		if (Options.bDrawPrimitives)
		{
			FRenderGraphRasterPassDesc GBuffer{};
			GBuffer.Name = "GBuffer";
			GBuffer.ColorAttachments = {
				{ BaseColor, ERenderGraphLoadAction::Clear, { 0.0f, 0.0f, 0.0f, 0.0f } },
				{ Normal, ERenderGraphLoadAction::Clear, { 0.0f, 0.0f, 0.0f, 0.0f } }
			}; // GBuffer Pass 에는 BaseColor 와 Normal Texture가 Clear된 상태로 필요하다

			GBuffer.DepthAttachment = { Depth, ERenderGraphLoadAction::Clear, 1.0f, 0 }; // GBuffer Pass 에는 Depth Texture가 Clear된 상태로 필요하다


			if (!Graph.AddRasterPass(std::move(GBuffer), [this, Renderer]
				{
					PrepareDrawData(Renderer, RenderData);
					TArray<FMeshDrawBatch> Batches;
					FMeshPassProcessor Processor;
					bCorePassSucceeded = Processor.Build(View, RenderData.RenderQueue,
						&RenderData, EMeshPassType::GBuffer, Batches);
					if (bCorePassSucceeded)
					{
						Renderer->UploadViewMatrix(View.ViewProjectionMatrix);
						Renderer->DrawMeshes(Batches);
					}
				})) return false;

			FRenderGraphRasterPassDesc LightingPass{};
			LightingPass.Name = "DeferredLighting";
			LightingPass.ReadTextures = { BaseColor, Normal, Depth };
			LightingPass.ColorAttachments = {
				{ LightingHDR, ERenderGraphLoadAction::Load }
			};
			if (!Graph.AddRasterPass(std::move(LightingPass), [this, Renderer, &Graph, BaseColor, Normal, Depth, Width, Height]
				{
					if (!bCorePassSucceeded) return;
					if (!BindDeferredLighting(Renderer, Width, Height))
					{
						bCorePassSucceeded = false;
						return;
					}
					const std::array<FTexture2D*, 3> Textures =
					{ Graph.Resolve(BaseColor), Graph.Resolve(Normal), Graph.Resolve(Depth) };
					if (FPipelineState* Pipeline = FRenderResourceManager::GetPSO(EPSOType::DeferredLighting);
						Pipeline && Pipeline->Shader && Pipeline->Shader->VertexShader && Pipeline->Shader->PixelShader)
						Renderer->DrawFullscreen(*Pipeline, Textures);
					else
						bCorePassSucceeded = false;
					Renderer->UnbindResources(3, 2, EShaderBindFlagBits::Pixel);
				})) return false;
		}
		else
		{
			FRenderGraphRasterPassDesc ClearDepth{};
			ClearDepth.Name = "ClearSceneDepth";
			ClearDepth.DepthAttachment = { Depth, ERenderGraphLoadAction::Clear, 1.0f, 0 };
			if (!Graph.AddRasterPass(std::move(ClearDepth), [] {})) return false;
		}
	}
	else // Forward Rendering
	{
		ForwardBatches.Reset();
		if (Options.bDrawPrimitives)
		{
			FMeshPassProcessor Processor;
			if (!Processor.Build(View, RenderData.RenderQueue, &RenderData,
				EMeshPassType::Forward, ForwardBatches, Options.LightingModel)) return false;
		}
		// 현재 업로드된 광원 정보를 기존 그리드에 전달한다.
		FForwardLightTileGrid& Tiles = RenderData.ViewLightResources.GetTiles();
		FStructuredBuffer* SceneLights = RenderData.ViewLightResources.GetSceneLights();
		FForwardLightingConstants GridLighting{};
		std::memcpy(GridLighting.LightCounts, LightCounts, sizeof(LightCounts));
		std::memcpy(GridLighting.LightOffsets, LightOffsets, sizeof(LightOffsets));

		// 픽셀 조명 경로에서만 목록을 사용한다.
		const ELightCullingMode CullingMode = !View.bIsWireframe && Options.LightingModel != ELightingModel::Gouraud ? Options.LightCullingMode : ELightCullingMode::Off;

		// Off도 이번 프레임의 화면 정보와 입력 광원 개수를 갱신한다.
		bool bUseDepthPrepass = false;
		const bool bGridPrepared = Options.bDrawPrimitives && bLightUploadSucceeded &&
			Tiles.PrepareBuild(View, GridLighting, SceneLights, Targets.Depth.get(), CullingMode);

		const bool bHasShadows = ShadowRef && !RenderData.ProjectedShadows.IsEmpty();
		const bool bNeedCulling = bGridPrepared && CullingMode != ELightCullingMode::Off && (LightCounts[0] || LightCounts[1] || LightCounts[3]);
		bUseDepthPrepass = Tiles.RequiresDepth() || bHasShadows;

		if (bUseDepthPrepass)
		{
			// 2.5D만 타일의 실제 깊이 범위가 필요하므로 Depth Prepass를 먼저 그린다.
			FPipelineState* DepthPipeline = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_DepthOnly);
			if (!DepthPipeline || !DepthPipeline->Shader || !DepthPipeline->Shader->PixelShader) return false;
			bUseDepthPrepass = true;

			// ForwardOpaque에서 이 깊이를 재사용한다.
			FRenderGraphRasterPassDesc Prepass{};
			Prepass.Name = "DepthPrepass";
			Prepass.DepthAttachment = { Depth, ERenderGraphLoadAction::Clear, 1.0f, 0 };
			if (!Graph.AddRasterPass(std::move(Prepass), [this, Renderer, DepthPipeline]
				{
					PrepareDrawData(Renderer, RenderData);
					if (ForwardBatches.IsEmpty()) return;

					// 실제 메시 패스와 같은 VS를 사용해 깊이 계산을 일치시킨다.
					FShaderProgram DepthProgram(ForwardBatches.Front().Pipeline.Shader->VertexShader, DepthPipeline->Shader->PixelShader);
					TArray<FMeshDrawBatch> DepthBatches = ForwardBatches;
					for (FMeshDrawBatch& Batch : DepthBatches)
					{
						Batch.Pipeline = *DepthPipeline;
						Batch.Pipeline.Shader = &DepthProgram;
					}
					Renderer->DrawMeshes(DepthBatches);
				}))
				return false;
		}

		if (bNeedCulling)
		{
			// 선택한 알고리즘에 맞는 컴퓨트 패스를 등록한다.
			FRenderGraphComputePassDesc Culling{};
			Culling.Name = CullingMode == ELightCullingMode::Clustered ? "ClusterLightCulling"
				: CullingMode == ELightCullingMode::Tiled25D ? "TileLightCulling25D" : "TileLightCulling";
			Culling.ReadResources = { SceneLights->GetBuffer() };
			if (Tiles.RequiresDepth())
				Culling.ReadResources.push_back(Targets.Depth->GetRawPtr());
			Culling.WriteResources = { Tiles.GetWriteResource() };
			if (!Graph.AddComputePass(std::move(Culling),
				[this, Renderer, SceneLights, DepthTexture = Targets.Depth.get()]
				{
					if (bCorePassSucceeded)
						bTilesBuilt = RenderData.ViewLightResources.GetTiles().ExecuteBuild(Renderer, DepthTexture, SceneLights);
				}))
				return false;
		}

		FRenderGraphTextureRef ShadowMask{};
		if (bHasShadows)
		{
			FRenderGraphTextureDesc MaskDesc{};
			MaskDesc.Width = Width;
			MaskDesc.Height = Height;
			MaskDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			ShadowMask = Graph.CreateTexture(MaskDesc, "ShadowMask");

			FRenderGraphRasterPassDesc Proj{};
			Proj.Name = "ShadowProjection";
			Proj.ReadTextures = { Depth, ShadowRef };
			Proj.ColorAttachments = { { ShadowMask, ERenderGraphLoadAction::Clear, { 1.0f, 1.0f, 1.0f, 1.0f } } };
			if (!Graph.AddRasterPass(std::move(Proj), [this, Renderer, &Graph, Depth, ShadowRef, ShadowMap, Width, Height]
				{
					FPipelineState* Pipeline = FRenderResourceManager::GetPSO(EPSOType::ShadowProjection);

					FConstantBuffer* CB = RenderData.ViewLightResources.GetShadowProjectionConstants();
					if (!Pipeline || !Pipeline->Shader || !CB || !ShadowMap) return;

					const float MapSize = static_cast<float>(ShadowMap->GetWidth());
					for (const FVisibleLightInfo& Info : RenderData.VisibleLightInfos)
						for (const FProjectedShadowInfo* Shadow : Info.ShadowsToProject)
						{
							int32 Channel = Shadow->LightSceneInfo->GetDynamicShadowMapChannel();
							if (!Shadow->bAllocated || Channel < 0) continue;

							FShadowProjectionConstants C{};
							C.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
							C.ShadowViewProjection = Shadow->ShadowViewProjection;
							C.ShadowUVScaleOffset = FVector4(Shadow->ResolutionX / MapSize, Shadow->ResolutionY / MapSize,
								Shadow->X / MapSize, Shadow->Y / MapSize);
							C.ShadowParams = FVector4(1.0f / MapSize, Shadow->GetShaderDepthBias(), 0.0f, 0.0f);
							C.ViewportSize = FVector4(static_cast<float>(Width), static_cast<float>(Height), 0.0f, 0.0f);
							C.CascadeParams = Shadow->bDirectionalLight
								? FVector4(Shadow->CascadeSettings.SplitNear, Shadow->CascadeSettings.SplitFar, 0.0f, 0.0f)
								: FVector4(0.0f, FLT_MAX, 0.0f, 0.0f);   // 구간 제한 없음
							C.CameraPosition = FVector4(View.ViewLocation, 1.0f);
							C.CameraForward = FVector4(View.ViewForward);
							if (Shadow->bDirectionalLight)
							{
								// 언리얼: GetLightSourceAngle은 전체 각도 -> 절반의 tan
								const float TanHalf = std::tan(0.5f * FMath::DegreesToRadians(
									Shadow->LightSceneInfo->Proxy->GetLightSourceAngle()));
								const float SW = 2.0f * Shadow->ShadowBounds.Radius;              // 그림자 맵 폭 (월드)
								const float SZ = Shadow->MaxSubjectZ - Shadow->MinSubjectZ;       // 깊이 범위 (월드)
								constexpr float MaxKernelTexels = 40.0f;                         // 언리얼 r.Shadow.MaxSoftKernelSize 기본값
								C.PCSSParams = FVector4(TanHalf * SZ / SW, MaxKernelTexels / Shadow->ResolutionX, 0.0f, 0.0f);
							}
							else
							{
								C.PCSSParams = FVector4(0.0f, 0.0f, 0.0f, 0.0f);   // Spot/Point는 기존 3×3 PCF
							}

							Renderer->UpdateConstant(CB, &C, sizeof(C));
							Renderer->BindPixelConstant(0, CB);
							Renderer->BindSampler(0, ESamplerState::ShadowCompare, EShaderBindFlagBits::Pixel);
							const std::array<FTexture2D*, 2> Textures = { Graph.Resolve(Depth), Graph.Resolve(ShadowRef) };
							FPipelineState ChannelPSO = *Pipeline;
							ChannelPSO.BlendState = static_cast<EBlendState>((uint8)EBlendState::ShadowMaskR + Channel);
							Renderer->DrawFullscreen(ChannelPSO, Textures);
						}
				})) return false;
		}

		FRenderGraphRasterPassDesc Forward{};
		Forward.Name = "ForwardOpaque";
		if (ShadowMask) Forward.ReadTextures = { ShadowMask };
		Forward.ColorAttachments = {
			{ LightingHDR, ERenderGraphLoadAction::Load }
		};
		Forward.DepthAttachment = { Depth, bUseDepthPrepass ? ERenderGraphLoadAction::Load : ERenderGraphLoadAction::Clear, 1.0f, 0 };
		if (!Graph.AddRasterPass(std::move(Forward), [this, Renderer, &Graph, ShadowMask, LightingOptions = Options, bUseDepthPrepass]
			{
				if (!LightingOptions.bDrawPrimitives) return;
				if (!bUseDepthPrepass) PrepareDrawData(Renderer, RenderData);
				bCorePassSucceeded = BindForwardLighting(Renderer, LightingOptions);
				if (!bCorePassSucceeded) return;

				FTexture2D* Mask = ShadowMask ? Graph.Resolve(ShadowMask) : nullptr;
				ID3D11ShaderResourceView* MaskSRV[] = { Mask ? Mask->GetSRV() : nullptr };
				Renderer->BindRawPixelResources(13, MaskSRV);

				if (bUseDepthPrepass)
					for (FMeshDrawBatch& Batch : ForwardBatches)
						Batch.Pipeline.DepthStencilState = EDepthStencilState::PrepassReadOnly;
				Renderer->DrawMeshes(ForwardBatches);
				Renderer->UnbindResources(13, 1, EShaderBindFlagBits::Pixel);
				UnbindForwardLighting(Renderer);
			})) return false;
	}

	FRenderGraphRasterPassDesc ToneMap{};
	ToneMap.Name = "ToneMap";
	ToneMap.ReadTextures = { LightingHDR };
	ToneMap.ColorAttachments = { { SceneColor, ERenderGraphLoadAction::Clear, { 0.0f, 0.0f, 0.0f, 1.0f } } };
	if (!Graph.AddRasterPass(std::move(ToneMap), [this, Renderer, &Graph, LightingHDR]
		{
			if (bCorePassSucceeded)
			{
				if (FPipelineState* Pipeline = FRenderResourceManager::GetPSO(EPSOType::ToneMap);
					Pipeline && Pipeline->Shader && Pipeline->Shader->VertexShader && Pipeline->Shader->PixelShader)
				{
					const std::array<FTexture2D*, 1> Textures = { Graph.Resolve(LightingHDR) };
					Renderer->DrawFullscreen(*Pipeline, Textures);
				}
			}
		})) return false;

	return true;
}

bool FSceneRenderer::AddViewportPasses(FRenderGraphBuilder& Graph, FRenderer* Renderer,
	FSceneViewTargets& Targets, const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox,
	const FScenePassCallbacks& Callbacks) // 뷰포트에 그려져야 되는 패스들을 옵션에 따라 정해진 순서로 Graph에 등록
{
	FSceneRenderOptions CoreOptions = Options;
	if (Options.bShowLightCulling)
	{
		// 디버그 보기에서 현재 뷰의 실제 타일 목록을 생성한다.
		CoreOptions.RenderingPath = ERenderingPath::Forward;
		CoreOptions.LightingModel = ELightingModel::Lambert;
	}
	if (!AddCorePasses(Graph, Renderer, Targets, CoreOptions, Skybox)) // 기본적으로 그려져야 되는 Core Pass 들
		return false;

	const FRenderGraphTextureRef SceneColor = Graph.ImportTexture(Targets.SceneColor.get(), "SceneColor");
	const FRenderGraphTextureRef Depth = Graph.ImportTexture(Targets.Depth.get(), "SceneDepth");
	if (!SceneColor || !Depth) return false;
	bHasFog = false;
	bFXAAPassSucceeded = true;

	if (!Options.bShowWorldNormal && Options.bDrawPrimitives && Options.bEnableFog && Callbacks.HeightFog)
	{
		FRenderGraphRasterPassDesc Pass = MakeViewportPass("HeightFog", SceneColor);
		Pass.ReadTextures = { Depth };
		if (!Graph.AddRasterPass(std::move(Pass), [this, DrawFog = Callbacks.HeightFog]
			{
				bHasFog = DrawFog();
			})) return false;
	}
	if (!Options.bShowWorldNormal && Options.bDrawPrimitives)
	{
		if (!Graph.AddRasterPass(MakeViewportPass("Translucent", SceneColor, Depth),
			[this, Renderer, GetFog = Callbacks.TranslucentFogConstants]
			{
				RenderTranslucent(Renderer, GetFog ? GetFog(bHasFog) : nullptr);
			})) return false;
	}
	if (!Options.bShowWorldNormal && Callbacks.SceneText &&
		!Graph.AddRasterPass(MakeViewportPass("SceneText", SceneColor, Depth), Callbacks.SceneText))
		return false;

	if (!Options.bShowWorldNormal && Options.bShowSceneDepth && Callbacks.DepthVisualization)
	{
		FRenderGraphRasterPassDesc Pass = MakeViewportPass("SceneDepthVisualization", SceneColor);
		Pass.ReadTextures = { Depth };
		if (!Graph.AddRasterPass(std::move(Pass), Callbacks.DepthVisualization)) return false;
	}

	FRenderGraphTextureRef FinalColor = SceneColor;
	if (!Options.bShowWorldNormal && Callbacks.FXAAOutput && Callbacks.FXAA)
	{
		FinalColor = Graph.ImportTexture(Callbacks.FXAAOutput, "FXAAOutput");
		if (!FinalColor) return false;
		FRenderGraphRasterPassDesc Pass = MakeViewportPass("FXAA", FinalColor);
		Pass.ReadTextures = { SceneColor };
		if (!Graph.AddRasterPass(std::move(Pass), [this, ApplyFXAA = Callbacks.FXAA]
			{
				bFXAAPassSucceeded = ApplyFXAA();
			})) return false;
	}

	if (Options.bShowLightCulling && Options.bDrawPrimitives && !Options.bShowWorldNormal)
	{
		FRenderGraphRasterPassDesc Debug = MakeViewportPass("LightCullingDebug", FinalColor);
		Debug.ReadTextures = { Depth };
		if (!Graph.AddRasterPass(std::move(Debug), [this, Renderer, &Graph, Depth]
			{
				if (!bCorePassSucceeded) return;
				FPipelineState* Pipeline = FRenderResourceManager::GetPSO(EPSOType::LightCullingDebug);
				if (!Pipeline || !Pipeline->Shader || !Pipeline->Shader->VertexShader || !Pipeline->Shader->PixelShader)
				{
					bCorePassSucceeded = false;
					return;
				}
				FForwardLightTileGrid& Tiles = RenderData.ViewLightResources.GetTiles();
				Tiles.PreparePixelShader(Renderer, bTilesBuilt);
				Renderer->BindPixelConstant(3, Tiles.GetPixelConstants());
				const std::array<ID3D11ShaderResourceView*, 1> TileResources = { Tiles.GetPixelResource() };
				Renderer->BindRawPixelResources(9, TileResources);
				const std::array<FTexture2D*, 1> Textures = { Graph.Resolve(Depth) };
				Renderer->DrawFullscreen(*Pipeline, Textures);
				Renderer->UnbindResources(9, 1, EShaderBindFlagBits::Pixel);
				Renderer->BindPixelConstant(3, nullptr);
				Tiles.ResetPixelState();
			})) return false;
	}

	const FSceneOverlayCallbacks& Overlay = Callbacks.Overlays;
	if (!AddOverlayPass(Graph, "EditorLines", FinalColor, Depth, Overlay.EditorLines) ||
		!AddOverlayPass(Graph, "EditorGrid", FinalColor, Depth, Overlay.EditorGrid) ||
		!AddOverlayPass(Graph, "EditorOutline", FinalColor, Depth, Overlay.EditorOutline) ||
		!AddOverlayPass(Graph, "EditorGizmo", FinalColor, Depth, Overlay.EditorGizmo, ERenderGraphLoadAction::Clear) ||
		!AddOverlayPass(Graph, "SIEOutline", FinalColor, Depth, Overlay.SIEOutline) ||
		!AddOverlayPass(Graph, "SIEGizmo", FinalColor, Depth, Overlay.SIEGizmo, ERenderGraphLoadAction::Clear))
		return false;
	if (Overlay.bClearEditorDepth)
	{
		FRenderGraphRasterPassDesc ClearDepth{};
		ClearDepth.Name = "EditorClearDepth";
		ClearDepth.DepthAttachment = { Depth, ERenderGraphLoadAction::Clear, 1.0f, 0 };
		if (!Graph.AddRasterPass(std::move(ClearDepth), [] {})) return false;
	}
	if (!AddOverlayPass(Graph, "EditorUUID", FinalColor, Depth, Overlay.EditorUUID)) return false;
	return true;
}


// 반투명 렌더링
void FSceneRenderer::RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants)
{
	if (!Renderer)
	{
		return;
	}

	Renderer->BindPixelConstant(3, FogConstants);
	PrepareDrawData(Renderer, RenderData);
	TArray<FMeshDrawBatch> Batches;
	FMeshPassProcessor Processor;
	if (Processor.Build(View, RenderData.RenderQueue, &RenderData,
		EMeshPassType::Translucent, Batches))
	{
		Renderer->UploadViewMatrix(View.ViewProjectionMatrix);
		Renderer->DrawMeshes(Batches);
	}
	Renderer->BindPixelConstant(3, nullptr);
}

void FSceneRenderer::GatherRenderPackets(FScene& Scene, FSceneRenderData& Data,
	const FViewContext& View, const FFrustumPlanes* Frustum, FRenderer* Renderer) //여기서 꼭 renderer를 매개변수로 받아야 할까?
{
	Data.ResetFrame();
	FRenderQueue& RenderQueue = Data.RenderQueue;
	FRenderStats& RenderStats = Data.RenderStats;
	auto& VisibleProxies = Data.VisibleProxies;
	auto& LODInputs = Data.LODInputs;
	auto& SelectedLODs = Data.SelectedLODs;
	auto& GatherChunks = Data.GatherChunks;
	using FGatherChunk = FSceneRenderData::FGatherChunk;

	{
		SCOPE_CYCLE_COUNTER(STAT_FrustumCull);
		// 표시 상태는 프록시 캐시로 판정해 오클루전과 드로우 수집 전에 제외한다.
		const auto Visit = [&](FPrimitiveSceneProxy* Proxy)
			{
				if (Proxy->IsShown(View.bGameView)) VisibleProxies.Add(Proxy);
			};

		if (Frustum)
		{
			Scene.BVH.QueryCull(
				FrustumAllPlanesMask,
				[Frustum](const FBox& Bounds, uint32& Mask)
				{
					return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, *Frustum, Mask));
				},
				Visit);
		}
		else
		{
			Scene.BVH.QueryCull(0, [](const FBox&, uint32&) { return EBVHCullResult::Inside; }, Visit);
		}
	}

	RenderStats.TotalPrimitives = Scene.PrimitiveSceneProxies.Num();

	// GPU 오클루전: 프러스텀을 통과한 물체를 GPU에서 가림 판정하고 결과를 이번 프레임에 받아 온다.
	// Cull이 켜져 있으면 가려진 물체를 목록에서 빼서 이후 Gather·정렬·드로우를 모두 건너뛴다.
	// 꺼져 있으면(검증 모드) 목록은 그대로 두고 패킷에 판정만 표시한다.
	const uint8* OccludedMask = nullptr;
	if (Renderer && Renderer->GetGPUOcclusion().GetSettings().bEnabled)
	{
		FGPUOcclusion& Occlusion = Renderer->GetGPUOcclusion();
		if (Occlusion.Run(VisibleProxies.GetData(), VisibleProxies.Num(), View))
		{
			const std::vector<uint8>& Occluded = Occlusion.GetOccluded();
			if (Occlusion.GetSettings().bCull)
			{
				uint32 Kept = 0;
				for (uint32 i = 0; i < VisibleProxies.Num(); ++i)
					if (!Occluded[i])
						VisibleProxies[Kept++] = VisibleProxies[i];
				VisibleProxies.SetNum(Kept);
			}
			else
			{
				OccludedMask = Occluded.data();
			}
		}
	}

	RenderStats.VisiblePrimitives = VisibleProxies.Num();

	constexpr uint32 ExtraSlots = 4096;
	const uint32 VisibleCount = VisibleProxies.Num();
	const uint32 MaxSlots = VisibleCount + ExtraSlots;
	if (Renderer) Data.ObjectConstants.SetNum(MaxSlots);
	const bool bPrepareObjectSlots = Renderer != nullptr;
	uint32 NextExtraSlot = VisibleCount;

	// 조각 수 = 스레드 수 × 4. 잘게 나눠야 먼저 끝난 스레드가 남은 조각을 가져가서 부하가 고르게 된다.
	const uint32 NumThreads = Tasks::FTaskScheduler::Get().GetNumWorkers() + 1;
	const uint32 ChunkCount = FMath::Clamp(VisibleCount, 1u, NumThreads * 4);

	if (GatherChunks.Num() < ChunkCount)
		GatherChunks.SetNum(ChunkCount);     // 늘릴 때만. 줄이지 않아야 배열 용량이 계속 재사용된다.

	// Renderer가 있으면 불투명 스태틱 메시는 패킷 대신 조각별 묶음에 작은 항목으로 넣는다.
	// 동일 바인딩의 개별 드로우를 묶어 패킷 복사와 항목별 정렬을 줄인다.
	const bool bStaticGroups = Renderer != nullptr;

	{
		SCOPE_CYCLE_COUNTER(STAT_GatherElements);

		LODInputs.Reset();
		LODInputs.Reserve(VisibleProxies.Num());
		for (const FPrimitiveSceneProxy* Proxy : VisibleProxies)
			LODInputs.Add({ Proxy->GetLODSphere(), Proxy->GetRenderState() });
		SelectLODs(LODInputs, View, SelectedLODs);
		Tasks::ParallelForChunks(VisibleCount, ChunkCount, [&](uint32 Begin, uint32 End, uint32 ChunkIndex)
			{
				FGatherChunk& Out = GatherChunks[ChunkIndex];      // 이 조각 전용. 다른 스레드는 절대 안 건드림
				// (머티리얼, 메시, LOD) 묶음 찾기. 조합이 몇 개뿐이라 선형 탐색이면 충분하고, 바로 전 묶음을 먼저 본다.
				const auto FindGroup = [&Out](UMaterial* Material, UStaticMesh* Mesh, uint8 LOD) -> FStaticDrawGroup&
					{
						if (Out.LastGroup < Out.Groups.size())
						{
							FStaticDrawGroup& Last = Out.Groups[Out.LastGroup];
							if (Last.Material == Material && Last.Mesh == Mesh && Last.LODIndex == LOD)
								return Last;
						}
						for (uint32 g = 0; g < Out.Groups.size(); ++g)
						{
							FStaticDrawGroup& Group = Out.Groups[g];
							if (Group.Material == Material && Group.Mesh == Mesh && Group.LODIndex == LOD)
							{
								Out.LastGroup = g;
								return Group;
							}
						}
						Out.LastGroup = static_cast<uint32>(Out.Groups.size());
						FStaticDrawGroup& Group = Out.Groups.emplace_back();
						Group.Material = Material;
						Group.Mesh = Mesh;
						Group.LODIndex = LOD;
						return Group;
					};

				for (uint32 VisibleIndex = Begin; VisibleIndex < End; ++VisibleIndex)
				{
					FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];   // 읽기만
					if (!Proxy->IsVisible()) continue;

					UStaticMesh* Mesh = Proxy->GetMesh();
					if (!Mesh)
					{
						Out.SlowPathIndices.Add(VisibleIndex);     // 컴포넌트를 건드리는 경로는 메인이 나중에
						continue;
					}

					const uint32 LOD = SelectedLODs[VisibleIndex];
					const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);
					++Out.LODCounts[LOD];                          // RenderStats 대신 조각 전용 통계

					uint32 Slot = InvalidObjectSlot;
					if (bPrepareObjectSlots)
					{
						// 칸 VisibleIndex는 이 반복만 쓴다 -> 스레드끼리 겹치지 않음
						const FPerObjectConstants Constants =
							MakePerObjectConstants(Proxy->GetLocalToWorld());

						Data.ObjectConstants[VisibleIndex] = Constants;

						Slot = VisibleIndex;
					}

					const bool bOccludedByGpu = OccludedMask && OccludedMask[VisibleIndex];
					for (uint32 i = 0; i < CachedLOD.NumSections; ++i)
					{
						const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + i);
						Out.LODTriangles[LOD] += Section.IndexCount / 3;

						if (bStaticGroups && Section.Material && Section.Material->PSOType == EPSOType::StaticMesh_Opaque)
						{
							FStaticDrawGroup& Group = FindGroup(Section.Material, Mesh, static_cast<uint8>(LOD));
							Group.Items.push_back({ Proxy, Slot, Section.StartIndex, Section.IndexCount, bOccludedByGpu ? 1u : 0u });
							++Out.StaticDrawCount;
							continue;
						}

						FRenderPacket& Packet = Out.Packets.AddDefaulted_GetRef();   // 조각 전용 배열에 추가
						Packet.Proxy = Proxy;
						Packet.Mesh = Mesh;
						Packet.Material = Section.Material;
						Packet.StartIndex = Section.StartIndex;
						Packet.IndexCount = Section.IndexCount;
						Packet.LODIndex = static_cast<uint8>(LOD);
						Packet.Slot = Slot;
						Packet.bOccludedByGpu = bOccludedByGpu;
					}
				}
			});

		// 이 뷰의 스태틱 드로우 수를 세고, 실제 렌더링할 때에만 해당 묶음을 바인딩한다.
		uint32 StaticDraws = 0;
		if (Renderer)
		{
			for (uint32 c = 0; c < ChunkCount; ++c)
			{
				StaticDraws += GatherChunks[c].StaticDrawCount;
			}
		}

		uint32 TotalPackets = 0;
		for (uint32 c = 0; c < ChunkCount; ++c)
			TotalPackets += GatherChunks[c].Packets.Num();

		RenderQueue.Reserve(TotalPackets + 256);             // 느린 경로 몫 약간 여유
		for (uint32 c = 0; c < ChunkCount; ++c)
			if (GatherChunks[c].Packets.Num() > 0)
				RenderQueue.Append(GatherChunks[c].Packets);  // 조각 순서대로 이어 붙이기

		for (uint32 c = 0; c < ChunkCount; ++c)
		{
			for (uint32 VisibleIndex : GatherChunks[c].SlowPathIndices)
			{
				FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];
				UPrimitiveComponent* Primitive = Proxy->GetComponent();
				if (!Primitive || !Primitive->IsVisible())
					continue;

				const uint32 FirstNew = RenderQueue.Num();

				// 프록시 캐시가 없는 스태틱 메시는 기존처럼 LOD를 골라 제출하고, 그 외는 컴포넌트에 맡긴다.
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
				if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
				{
					const uint32 LOD = SelectedLODs[VisibleIndex];
					StaticMeshComponent->SubmitToRenderQueue(RenderQueue, LOD);
				}
				else
				{
					Primitive->SubmitToRenderQueue(RenderQueue, View);
				}

				// continue 없이 항상 여기까지 와서 새 패킷에 여유 칸을 배정한다.
				for (uint32 p = FirstNew; p < RenderQueue.Num(); ++p)
				{
					if (!bPrepareObjectSlots || NextExtraSlot >= MaxSlots) break;
					FRenderPacket& Packet = RenderQueue[p];
					const FMatrix& Model = Packet.Proxy ? Packet.Proxy->GetLocalToWorld() : Packet.Model ? *Packet.Model : FMatrix::Identity;
					Data.ObjectConstants[NextExtraSlot] = MakePerObjectConstants(Model);
					Packet.Slot = NextExtraSlot++;
				}
			}
		}

		for (uint32 c = 0; c < ChunkCount; ++c)
			for (uint32 L = 0; L < 4; ++L)
			{
				RenderStats.LODCounts[L] += GatherChunks[c].LODCounts[L];
				RenderStats.LODTriangles[L] += GatherChunks[c].LODTriangles[L];
			}

		if (bPrepareObjectSlots) Data.ObjectConstants.SetNum(NextExtraSlot);
		RenderStats.DrawCalls = RenderQueue.Num() + StaticDraws;
		for (uint64 T : RenderStats.LODTriangles) RenderStats.Triangles += T;
	}
}


void FSceneRenderer::PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data)
{
	if (!Renderer) return;
	if (uint8* Destination = Renderer->BeginObjectConstants(static_cast<uint32>(Data.ObjectConstants.Num())))
	{
		for (int32 Index = 0; Index < Data.ObjectConstants.Num(); ++Index)
			std::memcpy(Destination + static_cast<size_t>(Index) * ObjectSlotBytes,
				&Data.ObjectConstants[Index], sizeof(FPerObjectConstants));
		Renderer->EndObjectConstants();
	}
}
