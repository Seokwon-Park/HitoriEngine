#include "EnginePCH.h"
#include "Editor/Rendering/SceneDepthRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"

// Shader와 상수 버퍼를 준비한다.
void FSceneDepthRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/SceneDepthShader.hlsl");

	// depth buffer를 사용하지 않고, 별도의 shader를 통해 렌더링을 처리한다.
	SceneDepthPipelineState.Shader = Shader;
	SceneDepthPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SceneDepthPipelineState.RasterizerState = ERasterizerState::SolidNone;
	SceneDepthPipelineState.BlendState = EBlendState::Opaque;
	SceneDepthPipelineState.DepthStencilState = EDepthStencilState::Disabled;

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FSceneDepthData));
}

// Depth Buffer에 있는 값을 바탕으로 viewport 전체를 회색값으로 그린다.
bool FSceneDepthRenderer::OnRender(const FSceneView& View, FTexture2D* DepthTarget, FTexture2D* ColorTarget, float MaxRange)
{
	FSceneDepthData Data{};
	Data.A = View.ProjectionMatrix.M[0][2];
	Data.B = View.ProjectionMatrix.M[3][2];
	Data.MaxRange = MaxRange;
	Data.bIsPerspective = View.bIsPerspective;

	RenderCommand::BindPipelineState(SceneDepthPipelineState);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, DepthTarget, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(6);
	RenderCommand::BindShaderResource(0, static_cast<FTexture2D*>(nullptr), EShaderBindFlagBits::Pixel);
	return true;
}
