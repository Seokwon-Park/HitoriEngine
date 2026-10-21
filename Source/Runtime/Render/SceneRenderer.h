#pragma once

#include "Core/Types.h"
#include "Render/SceneView.h"
#include "Render/RenderPacket.h"
#include "Render/MeshPassProcessor.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneRenderOptions.h"
#include "Render/RenderGraph/RenderGraph.h"

class UWorld;
class FScene;
class FRenderer;
class FConstantBuffer;
class FTexture2D;
class FSkyboxRenderer;
struct FSceneViewTargets;

// 에디터는 실행할 작업만 제공한다. 패스 생성과 순서는 FSceneRenderer가 결정한다.
struct FSceneOverlayCallbacks
{
	std::function<void()> EditorLines;
	std::function<void()> EditorGrid;
	std::function<void()> EditorOutline;
	std::function<void()> EditorGizmo;
	std::function<void()> SIEOutline;
	std::function<void()> SIEGizmo;
	bool bClearEditorDepth = false;
	std::function<void()> EditorUUID;
};

struct FScenePassCallbacks // 기본 도형 다 그려진 후 그려져야 될 목록들
{
	std::function<bool()> HeightFog;
	std::function<FConstantBuffer*(bool)> TranslucentFogConstants;
	std::function<void()> SceneText;
	std::function<void()> DepthVisualization;
	std::function<bool()> FXAA;
	FTexture2D* FXAAOutput = nullptr;
	FSceneOverlayCallbacks Overlays;
};



// 씬 렌더링 조율자
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData);
	~FSceneRenderer() = default;
	FSceneRenderer(const FSceneRenderer&) = delete;
	FSceneRenderer& operator=(const FSceneRenderer&) = delete;

	// 가시성 검사 및 렌더 큐 수집
	void InitViews(FRenderer* Renderer);
	void InitDynamicShadows();

	// 뷰 결과를 하나 유지하는 벤치마크에서도 사용한다.
	static void GatherRenderPackets(FScene& Scene, FSceneRenderData& Data, const FViewContext& View,
		const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);
	static void PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data);
	const FRenderStats& GetRenderStats() const { return RenderData.RenderStats; }

	// 한 뷰의 모든 패스를 정해진 순서로 등록한다.
	bool AddViewportPasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox, const FScenePassCallbacks& Callbacks);
	bool DidCorePassSucceed() const { return bCorePassSucceeded; }
	bool DidFXAAPassSucceed() const { return bFXAAPassSucceeded; }

	// 반투명 렌더링
	void RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants = nullptr);

	// 전체 렌더 큐 조회
	FRenderQueue& GetRenderQueue() { return RenderData.RenderQueue; }
	const FRenderQueue& GetRenderQueue() const { return RenderData.RenderQueue; }

	// 뷰 정보 조회
	const FSceneView& GetView() const { return View; }

	bool IsLightInView(FLightSceneProxy* Proxy);

	void CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo);
	void CreateViewDependentWholeSceneShadows(FLightSceneInfo* LightSceneInfo);

	void AllocateShadowDepthTargets();
	
private:
	bool AddCorePasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox);
	bool AddWorldNormalPass(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options);
	bool BindDeferredLighting(FRenderer* Renderer, uint32 Width, uint32 Height);
	bool BindForwardLighting(FRenderer* Renderer, const FSceneRenderOptions& Options);
	void UnbindForwardLighting(FRenderer* Renderer);
	bool IntersectSphereFrustum(const FVector4& PositionRadius, const FPlane (&Planes)[6]) const;

	void AddInteractingPrimitives(FProjectedShadowInfo* Shadow);

	UWorld* World = nullptr;
	FSceneView View;
	FSceneRenderData& RenderData;
	uint32 LightCounts[4]{};
	uint32 LightOffsets[4]{};
	bool bLightUploadSucceeded = false;
	bool bTilesBuilt = false;
	TArray<FMeshDrawBatch> ForwardBatches;
	FViewContext CachedViewContext;
	bool bCorePassSucceeded = true;
	bool bFXAAPassSucceeded = true;
	bool bHasFog = false;
};
