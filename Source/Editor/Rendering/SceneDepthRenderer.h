#pragma once

#include "Render/Renderer.h"
#include "Render/RenderingInfo.h"
#include "Render/PipelineState.h"

// 선형복원을 하기 위해 필요한 값을 넘긴다.
struct alignas(16) FSceneDepthData
{
	float A;
	float B;
	float MaxRange;
	uint32 bIsPerspective;
};

class FSceneDepthRenderer
{
public:
	// Shader와 상수 버퍼를 준비한다.
	void Init(FRenderer* InRenderer);
	// Depth Buffer에 있는 값을 바탕으로 viewport 전체를 회색값으로 그린다.
	bool OnRender(const FSceneView& View, FTexture2D* DepthTarget, FTexture2D* ColorTarget, float MaxRange);

private:
	FRenderer* Renderer;

	FShaderProgram* Shader;
	FPipelineState SceneDepthPipelineState;

	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
