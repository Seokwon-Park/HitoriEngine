#pragma once

#include "Render/Renderer.h"
#include "Render/RenderingInfo.h"
#include "Render/PipelineState.h"
#include "Editor/Rendering/Outline.h"

// Outline의 월드·투영 행렬과 View별 픽셀 크기·확장 두께를 담는다.
struct FOutlineData
{
	FMatrix World;
	FMatrix NormalMatrix;
	FMatrix ViewProj;
	FVector4 ViewportAndThickness;
};

class FOutlineRenderer
{
public:

	// 스텐실 마스크·외곽선 두 패스의 Shader와 상수 버퍼를 준비한다.
	void Init(FRenderer* InRenderer);
	// 외부에서 지정한 Mesh 참조를 저장한다.
	void SetMesh(UStaticMesh* InMesh);
	// 원본 메시로 스텐실을 찍은 뒤, 확장 메시를 그 바깥에만 그려 외곽선만 남긴다.
	void OnRender(const FOutline& InOutline, const FMatrix& ViewProj, const FViewportSettings& Viewport);

private:
	FRenderer* Renderer;

	FShaderProgram* Shader;
	FPipelineState MaskPipelineState;
	FPipelineState OutlinePipelineState;
	UStaticMesh* Mesh;

	ComPtr<ID3D11RasterizerState> RasterizerState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
