#include "EnginePCH.h"
#include "Editor/Rendering/HeightFogRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Texture2D.h"
#include <cmath>

void FHeightFogRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/HeightFogShader.hlsl");
	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FHeightFogData));
}

bool FHeightFogRenderer::OnRender(const FSceneView& View, FTexture2D* DepthTarget, FTexture2D* ColorTarget, const FHeightFogSetting& Settings)
{
	if (!DepthTarget || !ColorTarget || !Shader || !Shader->VertexShader ||
		!Shader->PixelShader || !ConstantBuffer)
		return false;

	const uint32 Width = ColorTarget->GetWidth();
	const uint32 Height = ColorTarget->GetHeight();
	if (Width == 0 || Height == 0 || DepthTarget->GetWidth() != Width ||
		DepthTarget->GetHeight() != Height)
		return false;

	if (Settings.FogDensity <= 0.0f || Settings.FogMaxOpacity <= 0.0f) 
		return false;

	FHeightFogData Data{};
	Data.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Data.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y, View.ViewLocation.Z, 0.0f);
	Data.FogColor = FVector4(FMath::Clamp(Settings.FogColor.X, 0.0f, 1.0f), FMath::Clamp(Settings.FogColor.Y, 0.0f, 1.0f), 
							 FMath::Clamp(Settings.FogColor.Z, 0.0f, 1.0f), 1.0f);
	Data.DensityHeight = FVector4(Settings.FogDensity, std::max(Settings.FogHeightFalloff, 0.0f),
								  Settings.FogHeight, std::max(Settings.StartDistance, 0.0f));
	Data.DistanceViewport = FVector4(FMath::Clamp(Settings.FogMaxOpacity, 0.0f, 1.0f),
									 std::max(Settings.FogCutoffDistance, 0.0f), 1.0f / Width, 1.0f / Height);

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, DepthTarget, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(6);
	RenderCommand::BindShaderResource(0, static_cast<FTexture2D*>(nullptr), EShaderBindFlagBits::Pixel);
	return true;
}

FConstantBuffer* FHeightFogRenderer::GetTranslucentFogConstants(bool bFogRendered)
{
	if (!ConstantBuffer)
		return nullptr;
	if (!bFogRendered)
	{
		const FHeightFogData Disabled{};
		RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Disabled, sizeof(Disabled));
	}
	return ConstantBuffer.get();
}
