#pragma once

#include "Render/LightRenderData.h"
#include "Render/ForwardLightTiles.h"
#include "Render/Buffer.h"

struct FDeferredLightConstants
{
	FMatrix InverseViewProjection;
	FVector4 CameraPosition;
	FVector4 CameraForward;
	FVector4 ViewportSize;
	FAmbientLightRenderData Ambient;
	uint32 LightCounts[4];
	uint32 LightOffsets[4];
	FVector4 SkyColorIntensity;
};

// 뷰별 조명 GPU 리소스. 패스나 드로우는 실행하지 않는다.
class FSceneLightResources
{
public:
	bool Init();
	// 광원을 종류별 구간(방향광, 포인트, 스포트, 렉트 순)으로 정렬해 업로드한다.
	// Counts/Offsets 순서는 x: 포인트, y: 스포트, z: 방향광, w: 렉트다.
	bool UploadViewLights(const TArray<FLightRenderParameters>& Lights,
		uint32 (&Counts)[4], uint32 (&Offsets)[4]);

	FStructuredBuffer* GetSceneLights() const { return SceneLightBuffer.get(); }
	FConstantBuffer* GetForwardConstants() const { return ForwardLightCB.get(); }
	FConstantBuffer* GetDeferredConstants() const { return DeferredLightCB.get(); }
	FConstantBuffer* GetShadowProjectionConstants() const { return ShadowProjectionLightCB.get(); }
	FForwardLightTileGrid& GetTiles() { return ForwardTileGrid; }


private:
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	TUniquePtr<FConstantBuffer> ShadowProjectionLightCB;
	FForwardLightTileGrid ForwardTileGrid;

	bool bInitialized = false;
};
