#pragma once

#include "Render/SceneView.h"
#include "Math/Vector4.h"
#include "Render/Buffer.h"
#include "Render/PipelineState.h"

class FRenderer;
class FTexture2D;

struct alignas(16) FHeightFogData
{
	FMatrix InverseViewProjection;
	FVector4 CameraPosition;
	FVector4 FogColor;
	FVector4 DensityHeight; // 밀도, 높이 감쇠, 기준 높이, 시작 거리
	FVector4 DistanceViewport; // 최대 불투명도, 차단 거리, 1/너비, 1/높이
};

// 컴포넌트에서 복사한 CPU 설정값. GPU 상수 버퍼 배치는 셰이더 구현 단계에서 정한다.
struct FHeightFogSetting
{
	float FogDensity = 0.0f;
	float FogHeightFalloff = 0.0f;
	float FogHeight = 0.0f;
	FVector4 FogColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	float StartDistance = 0.0f;
	float FogMaxOpacity = 1.0f;
	float FogCutoffDistance = 0.0f;
};

class FHeightFogRenderer
{
public:
	void Init(FRenderer* InRenderer);

	// 지원하는 뷰에서 깊이를 읽어 한 층 안개를 합성한다.
	bool OnRender(const FSceneView& View, FTexture2D* DepthTarget,
		FTexture2D* ColorTarget, const FHeightFogSetting& InHeightFogSetting);
	// 반투명 셰이더가 사용할 안개 데이터. 이 뷰에 안개가 없으면 0으로 초기화한다.
	FConstantBuffer* GetTranslucentFogConstants(bool bFogRendered);

private:
	FRenderer* Renderer = nullptr;
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
