#pragma once

#include "Math/EngineMath.h"
#include "Math/Sphere.h"
#include "Engine/SceneManagement.h"
#include "MeshPassProcessor.h"

class FPrimitiveSceneProxy;
struct FLightSceneInfo;
class FSceneRenderer;

inline constexpr uint32 ShadowAtlasSize = 2048;
inline constexpr uint32 ShadowAtlasTile = 512;
inline constexpr uint32 ShadowAtlasBorder = 2;    // PCF 3×3가 옆 칸을 읽지 않도록

struct FShadowProjectionConstants
{
	FMatrix  InverseViewProjection;   // 카메라
	FMatrix  ShadowViewProjection;    // 그림자 카메라
	FVector4 ShadowUVScaleOffset;     // xy: 칸 크기 / Shadow Map 크기, zw: 칸 위치 / Shadow Map 크기
	FVector4 ShadowParams;            // x: 1 / Shadow Map 크기 (PCF 간격), y: 비교 bias
	FVector4 ViewportSize;            // xy: 화면 크기
	FVector4 CascadeParams;   // x: SplitNear, y: SplitFar
	FVector4 CameraPosition;
	FVector4 CameraForward;
	FVector4 PCSSParams; // x: tan(half) * DepthRange / 그림자폭, y : 최대 커널 (칸 UV), 0 이면 PCSS 끔
};
static_assert(sizeof(FShadowProjectionConstants) == 240);

// 그림자 맵 한 장. 프레임마다 FSceneRenderData가 만들고 비운다.
class FProjectedShadowInfo
{
public:
	// 어느 라이트의 그림자인지
	FLightSceneInfo* LightSceneInfo = nullptr;

	FVector PreShadowTranslation;
	FMatrix ShadowView, ShadowProjection, ShadowViewProjection;
	FSphere ShadowBounds{ FVector::ZeroVector, 0.0f };
	FShadowCascadeSettings CascadeSettings;
	float MinSubjectZ = 0.0f;
	float MaxSubjectZ = 0.0f;
	// Shadow Map 안의 위치
	uint32 X = 0, Y = 0, ResolutionX = 0, ResolutionY = 0, BorderSize = 0;
	bool bAllocated = false;
	// 종류
	bool bWholeSceneShadow = false, bDirectionalLight = false, bOnePassPointLightShadow = false;
	// Caster
	TArray<FPrimitiveSceneProxy*> DynamicSubjectPrimitives;
	TArray<FMeshDrawBatch> ShadowDepthBatches;   // 언리얼의 ShadowDepthPass 역할

	FFrustumPlanes CasterFrustum{};

	void SetupWholeSceneProjection(FLightSceneInfo* InLightSceneInfo,
		const FWholeSceneProjectedShadowInitializer& Initializer,
		uint32 InResolutionX, uint32 InResolutionY, uint32 InBorderSize);
	void AddSubjectPrimitive(FPrimitiveSceneProxy* Proxy) { DynamicSubjectPrimitives.Add(Proxy); }
	float GetShaderDepthBias() const;
	float GetShaderSlopeDepthBias() const;

	void SetupMeshDrawCommandsForShadowDepth(FSceneRenderer& Renderer);
};
