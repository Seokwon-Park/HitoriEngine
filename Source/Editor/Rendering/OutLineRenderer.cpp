#include "EnginePCH.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"

// 스텐실 마스크·외곽선 두 패스의 Shader와 상수 버퍼를 준비한다.
// 두 패스 모두 깊이 테스트를 끄므로 다른 물체에 가려져도 외곽선이 보인다.
void FOutlineRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/OutlineShader.hlsl");

	// 1패스: 원본 크기 메시 영역을 색 없이 스텐실에만 기록한다.
	MaskPipelineState.Shader = Shader;
	MaskPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	MaskPipelineState.RasterizerState = ERasterizerState::SolidNone;
	MaskPipelineState.BlendState = EBlendState::NoColorWrite;
	MaskPipelineState.DepthStencilState = EDepthStencilState::StencilMask;

	// 2패스: 확장 메시를 스텐실이 비어 있는 곳에만 그려 테두리 띠만 남긴다.
	OutlinePipelineState.Shader = Shader;
	OutlinePipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	OutlinePipelineState.RasterizerState = ERasterizerState::SolidNone;
	OutlinePipelineState.BlendState = EBlendState::Opaque;
	OutlinePipelineState.DepthStencilState = EDepthStencilState::StencilOutline;

	static_assert(sizeof(FOutlineData) == 208, "Outline constant buffer layout mismatch");
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FOutlineData));
}

// 외부에서 지정한 Mesh 참조를 저장한다.
void FOutlineRenderer::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

// 카메라 거리 대신 View별 픽셀 크기와 고정 확장량으로 선택 Outline을 그린다.
void FOutlineRenderer::OnRender(const FOutline& InOutline, const FMatrix& InViewProj, const FViewportSettings& Viewport)
{
	if (!InOutline.GetTarget() || Viewport.Width == 0 || Viewport.Height == 0)
	{
		return;
	}

	// 메시 없는 컴포넌트(텍스트 등)는 아웃라인을 그리지 않는다
	if (!InOutline.GetMesh() || !InOutline.GetMesh()->IndexBuffer)
	{
		return;
	}

	RenderCommand::BindMesh(InOutline.GetMesh());
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);

	const FMatrix OriginalWorld = InOutline.GetWorldMatrix();
    const FMatrix& World = OriginalWorld;
    const FMatrix NormalMatrix = OriginalWorld.Inverse().GetTransposed();
    const FMatrix& ViewProj = InViewProj;
	const float ViewportWidth = static_cast<float>(Viewport.Width);
	const float ViewportHeight = static_cast<float>(Viewport.Height);
	const uint32 IndexCount = InOutline.GetMesh()->IndexBuffer->GetIndexCount();

	// 1패스: 두께 0(확장 없음)으로 선택 메시 영역을 스텐실에 찍는다.
	FOutlineData MaskConst = { World, NormalMatrix, ViewProj, FVector4(ViewportWidth, ViewportHeight, 0.0f, 0.0f) };
	RenderCommand::BindPipelineState(MaskPipelineState);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &MaskConst, sizeof(MaskConst));
	RenderCommand::DrawIndexed(IndexCount);

	// 2패스: 확장 메시 중 스텐실 바깥 부분만 노란색으로 그린다.
	FOutlineData OutlineConst = { World, NormalMatrix, ViewProj, FVector4(ViewportWidth, ViewportHeight, 2.0f, 0.0f) };
	RenderCommand::BindPipelineState(OutlinePipelineState);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &OutlineConst, sizeof(OutlineConst));
	RenderCommand::DrawIndexed(IndexCount);

	// 이후 패스(Gizmo 등)에 스텐실·블렌드 상태가 새지 않도록 기본값으로 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
}
