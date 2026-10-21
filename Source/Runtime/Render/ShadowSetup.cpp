#include "EnginePCH.h"
#include "ShadowRendering.h"
#include "SceneRenderer.h"



void FProjectedShadowInfo::SetupWholeSceneProjection(FLightSceneInfo* InLightSceneInfo, const FWholeSceneProjectedShadowInitializer& Initializer, uint32 InResolutionX, uint32 InResolutionY, uint32 InBorderSize)
{
	LightSceneInfo = InLightSceneInfo;
	PreShadowTranslation = Initializer.PreShadowTranslation;
	ResolutionX = InResolutionX;
	ResolutionY = InResolutionY;
	BorderSize = InBorderSize;
	bWholeSceneShadow = true;
	bOnePassPointLightShadow = Initializer.bOnePassPointLightShadow;
	CascadeSettings = Initializer.CascadeSettings;

	// 깊이 범위: Spot은 near = MinLightW, far = MaxDistanceToCastInLightW
	MinSubjectZ = Initializer.MinLightW;
	MaxSubjectZ = Initializer.MaxDistanceToCastInLightW;

	// 그림자 카메라를 텍셀 한 칸 단위로만 움직이게 해서, 텍셀 격자가 항상 월드의 같은 자리에 놓이게 한다
	// View: "라이트를 원점으로 옮기고(PreShadowTranslation) -> 라이트 방향으로 돌린다(WorldToLight)"
	// 엔진 투영 행렬이 X를 앞쪽으로 받으므로 언리얼의 FaceMatrix는 곱하지 않는다.
	if (Initializer.WAxis.W == 1.0f)   // Directional
	{
		const float Radius = 1.0f / Initializer.Scales.X;                  // 정사영 폭의 절반
		const float SnapResolution = static_cast<float>(ShadowAtlasTile - ShadowAtlasBorder * 2);   // 실제로 그리는 해상도
		const float TexelWorld = 2.0f * Radius / SnapResolution;

		// 월드 -> 라이트 공간 (회전만). X가 깊이, Y/Z가 그림자 맵의 가로/세로
		FVector LightSpace = Initializer.WorldToLight.TransformPosition(-PreShadowTranslation);
		LightSpace.Y = std::floor(LightSpace.Y / TexelWorld) * TexelWorld;
		LightSpace.Z = std::floor(LightSpace.Z / TexelWorld) * TexelWorld;

		// 라이트 -> 월드 (회전의 역 = 전치)
		PreShadowTranslation = -Initializer.WorldToLight.GetTransposed().TransformPosition(LightSpace);
	}
	ShadowView = FMatrix::MakeTranslation(PreShadowTranslation) * Initializer.WorldToLight;

	// Projection: Scales = 1 / tan(반각) -> FOV(도) = 2 * atan(1 / Scales)
	const float FOV = FMath::RadiansToDegrees(2.0f * std::atan(1.0f / Initializer.Scales.X));
	if (Initializer.WAxis.W == 1.0f)
	{
		bDirectionalLight = true;
		ShadowProjection = FMatrix::MakeOrthographLH(2.0f / Initializer.Scales.X, 2.0f / Initializer.Scales.Y, MinSubjectZ, MaxSubjectZ);
	}
	else
	{
		ShadowProjection = FMatrix::MakePerspectiveLH(FOV, 1.0f, MinSubjectZ, MaxSubjectZ);
	}

	ShadowViewProjection = ShadowView * ShadowProjection;   // 카메라와 같은 View * Proj 순서
	CasterFrustum = ExtractFrustumPlanes(ShadowViewProjection);

	// 언리얼(Directional이 아닐 때): 라이트 위치 중심, SubjectBounds 반지름
	ShadowBounds = FSphere(PreShadowTranslation * -1.0f, Initializer.SubjectBounds.SphereRadius);
}

float FProjectedShadowInfo::GetShaderDepthBias() const
{
	const float UserBias = LightSceneInfo->Proxy->GetUserShadowBias();
	const float DepthRange = MaxSubjectZ - MinSubjectZ;

	if (bDirectionalLight)
	{
		const float TexelWorld = 2.0f * ShadowBounds.Radius / ResolutionX;
		return 3.0f * UserBias * TexelWorld / DepthRange;
	}
	else
	{
		return 0.0005f;
	}

	return 0.0f;
}

void FProjectedShadowInfo::SetupMeshDrawCommandsForShadowDepth(FSceneRenderer& Renderer)
{
	FSceneView ShadowSceneView = Renderer.GetView();
	ShadowSceneView.ViewMatrix = ShadowView;
	ShadowSceneView.ProjectionMatrix = ShadowProjection;
	ShadowSceneView.ViewProjectionMatrix = ShadowViewProjection;
	ShadowSceneView.Frustum = CasterFrustum;
	ShadowSceneView.ViewLocation = PreShadowTranslation * -1.0f;   // 라이트 위치
	ShadowSceneView.bIsWireframe = false;

	FRenderQueue ShadowQueue;
	for (FPrimitiveSceneProxy* Proxy : DynamicSubjectPrimitives)
	{
		if (!Proxy->GetMesh() || !Proxy->GetRenderState()) continue;
		const uint32 LOD = 0;                                        // 처음엔 LOD0 고정
		const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);
		for (uint32 i = 0; i < CachedLOD.NumSections; ++i)
		{
			const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + i);
			// 반투명 재질은 그림자를 드리우지 않음 (불투명만)
			if (!Section.Material || Section.Material->PSOType != EPSOType::StaticMesh_Opaque) continue;

			FRenderPacket& Packet = ShadowQueue.AddDefaulted_GetRef();
			Packet.Proxy = Proxy;
			Packet.Mesh = Proxy->GetMesh();
			Packet.Material = Section.Material;
			Packet.StartIndex = Section.StartIndex;
			Packet.IndexCount = Section.IndexCount;
			Packet.LODIndex = static_cast<uint8>(LOD);
			Packet.Slot = InvalidObjectSlot;                      // 물체별 상수를 직접 넣으므로 슬롯 불필요
		}
	}
	ShadowQueue.Sort();   // 필수: FirstTranslucentIndex를 계산한다

	// 3. 그림자 깊이용 그릴 목록
	FMeshPassProcessor Processor;
	Processor.Build(ShadowSceneView, ShadowQueue, /*SceneData*/ nullptr,
		EMeshPassType::CSMShadowDepth, ShadowDepthBatches, ELightingModel::Lambert);
}

void FSceneRenderer::CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo)
{
	FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
	FVisibleLightInfo& VisibleLightInfo = RenderData.VisibleLightInfos[LightSceneInfo->Id];

	if (Proxy->GetShadowResolutionScale() <= 0.0f) return;

	TArray<FWholeSceneProjectedShadowInitializer> ProjectedShadowInitializers;
	if (!Proxy->GetWholeSceneProjectedShadowInitializer(View, ProjectedShadowInitializers)) return;

	const float Desired = ShadowAtlasTile * Proxy->GetShadowResolutionScale();
	uint32 Size = 1;
	while (Size << 1 <= static_cast<uint32>(Desired)) Size <<= 1u;
	Size = std::clamp(Size, 16u, ShadowAtlasTile);

	for (const FWholeSceneProjectedShadowInitializer& Initializer : ProjectedShadowInitializers)
	{
		RenderData.ProjectedShadows.Add(MakeUnique<FProjectedShadowInfo>());
		FProjectedShadowInfo* Shadow = RenderData.ProjectedShadows.Last().get();
		Shadow->SetupWholeSceneProjection(LightSceneInfo, Initializer, Size, Size, /*Border*/0);

		AddInteractingPrimitives(Shadow);
		Shadow->SetupMeshDrawCommandsForShadowDepth(*this);
		if (Shadow->DynamicSubjectPrimitives.Num() == 0) continue;   // 그릴 물체가 없으면 버림 

		VisibleLightInfo.AllProjectedShadows.Add(Shadow);
		VisibleLightInfo.ShadowsToProject.Add(Shadow);
	}
}

void FSceneRenderer::CreateViewDependentWholeSceneShadows(FLightSceneInfo* LightSceneInfo)
{
	FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
	FVisibleLightInfo& VisibleLightInfo = RenderData.VisibleLightInfos[LightSceneInfo->Id];

	if (Proxy->GetShadowResolutionScale() <= 0.0f) return;

	const float Desired = ShadowAtlasTile * Proxy->GetShadowResolutionScale();
	uint32 Size = 1;
	while (Size << 1 <= static_cast<uint32>(Desired)) Size <<= 1u;
	Size = std::clamp(Size, 16u, ShadowAtlasTile);

	const uint32 NumCascades = Proxy->GetNumViewDependentWholeSceneShadows(View);
	for (uint32 Cascade = 0; Cascade < NumCascades; ++Cascade)
	{
		FWholeSceneProjectedShadowInitializer ProjectedShadowInitializer;
		if (!Proxy->GetViewDependentWholeSceneProjectedShadowInitializer(View, Cascade, ProjectedShadowInitializer)) continue;

		RenderData.ProjectedShadows.Add(MakeUnique<FProjectedShadowInfo>());
		FProjectedShadowInfo* Shadow = RenderData.ProjectedShadows.Last().get();
		Shadow->SetupWholeSceneProjection(LightSceneInfo, ProjectedShadowInitializer, Size, Size, /*Border*/0);

		AddInteractingPrimitives(Shadow);
		Shadow->SetupMeshDrawCommandsForShadowDepth(*this);
		if (Shadow->DynamicSubjectPrimitives.Num() == 0) continue;   // 그릴 물체가 없으면 버림 

		VisibleLightInfo.AllProjectedShadows.Add(Shadow);
		VisibleLightInfo.ShadowsToProject.Add(Shadow); 
	}
}

void FSceneRenderer::AllocateShadowDepthTargets()
{
	constexpr uint32 TilesPerRow = ShadowAtlasSize / ShadowAtlasTile;   // 4 -> 16칸
	uint32 Next = 0;
	for (FVisibleLightInfo& Info : RenderData.VisibleLightInfos)
		for (FProjectedShadowInfo* Shadow : Info.ShadowsToProject)
		{
			if (Next >= TilesPerRow * TilesPerRow)
				return;                                  // 꽉 참: 나머지는 bAllocated = false로 남음
			Shadow->X = (Next % TilesPerRow) * ShadowAtlasTile + ShadowAtlasBorder;
			Shadow->Y = (Next / TilesPerRow) * ShadowAtlasTile + ShadowAtlasBorder;
			Shadow->ResolutionX = Shadow->ResolutionY = ShadowAtlasTile - ShadowAtlasBorder * 2;
			Shadow->BorderSize = ShadowAtlasBorder;
			Shadow->bAllocated = true;
			++Next;
		}
}
