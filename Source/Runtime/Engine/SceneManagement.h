#pragma once

#include "Math/EngineMath.h"
#include "Math/Color.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Frustum.h"

// 모든 광원(방향광·포인트·스포트·렉트)이 공유하는 GPU 원소.
// LightData.hlsli의 FLightRenderParameters와 바이트 단위로 같아야 한다.
struct FLightRenderParameters
{
	FVector Position;
	float Radius;
	FVector Color;
	float Intensity;
	FVector Direction;
	float InnerConeCos;
	FVector Right;
	float FalloffExponent;
	float OuterConeCos;
	float HalfWidth;
	float HalfHeight;
	uint32 Type;
	// Rect 전용: 발광면 가장자리의 문(Barn Door)
	float BarnDoorCos;
	float BarnDoorSin;
	float BarnDoorLength;
	int32 ShadowMapChannel;
};

static_assert(sizeof(FLightRenderParameters) == 96, "sizeof(LightData.hlsli FLightRenderParameters) == sizeof(FLightRenderParameters)");

// Directional 라이트 CSM에서 캐스케이드 하나가 맡는 구간과 Fade 정보
class FShadowCascadeSettings
{
public:
	// SplitNear <= FadePlane <= SplitFar (카메라 기준 거리)

	// 카메라에서 이 캐스케이드 앞쪽 경계까지의 거리
	float SplitNear = 0.0f;

	// 카메라에서 이 캐스케이드 뒤쪽 경계까지의 거리
	float SplitFar = 0.0f;

	// 인접 캐스케이드와 블렌딩하는 구간
	// 앞쪽 경계의 fade 폭
	float SplitNearFadeRegion = 0.0f;

	// 뒤쪽 경계의 fade 폭
	float SplitFarFadeRegion = 0.0f;

	// 다음 캐스케이드와 섞기 시작하는 거리. FadePlane~SplitFar 구간을 섞어 경계를 숨긴다.
	float FadePlaneOffset = 0.0f;

	// 섞는 구간의 길이 (SplitFar - FadePlaneOffset)
	float FadePlaneLength = 0.0f;

	// Caster 컬링용 볼록 영역. 필요해지면 FFrustumPlanes 등으로 추가한다.
	//FConvexVolume ShadowBoundsAccurate;

	FPlane NearFrustumPlane{};
	FPlane FarFrustumPlane{};

	// 원거리 전용 캐스케이드 여부 (bCastFarShadows가 켜진 물체만 그림)
	bool bFarShadowCascade;

	// Directional이면 캐스케이드 번호, Point면 큐브 면 번호, 그 외에는 INDEX_NONE
	int32 ShadowSplitIndex;

	// 캐스케이드별 깊이 bias 세기
	float CascadeBiasDistribution;

	FShadowCascadeSettings()
		: SplitNear(0.0f)
		, SplitFar(WORLD_MAX)
		, SplitNearFadeRegion(0.0f)
		, SplitFarFadeRegion(0.0f)
		, FadePlaneOffset(SplitFar)
		, FadePlaneLength(SplitFar - FadePlaneOffset)
		, bFarShadowCascade(false)
		, ShadowSplitIndex(INDEX_NONE)
		, CascadeBiasDistribution(1)
	{
	}
};

// 그림자 카메라에 대한 설계도, 이동, 회전, Scales, near/far, 덮는 영역
class FProjectedShadowInitializer
{
public:

	// 라이트를 원점으로 가져오려면 월드를 얼마나 옮겨야 하나?
	FVector PreShadowTranslation;

	// 카메라의 방향에 해당
	FMatrix WorldToLight;
	
	//시야 넓이
	FVector2 Scales;

	// 이 그림자가 덮는 공간
	FBoxSphereBounds SubjectBounds{};

	// 원근인지 직교인지
	FVector4 WAxis;

	// near
	float MinLightW = 0.0f;
	// far
	float MaxDistanceToCastInLightW = 0.0f;

	// IsCachedShadowValid는 그림자 캐싱에서 추가
};


// FProjectedShadowInitializer + 캐스케이드 정보 + Point 큐브 원패스 여부
class FWholeSceneProjectedShadowInitializer : public FProjectedShadowInitializer
{
public:
	// Directional 라이트의 CSM(Cascaded Shadow Maps) 정보.이 그림자가 몇 번째 캐스케이드이고 카메라 깊이 범위 중 어디를 담당하는지 
	FShadowCascadeSettings CascadeSettings;
	// 포인트 라이트의 큐브 그림자 6면을 한번에 드로우 할 것인지 여부.
	bool bOnePassPointLightShadow = false;
};
