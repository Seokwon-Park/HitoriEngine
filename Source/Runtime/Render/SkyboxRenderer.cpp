#include "EnginePCH.h"
#include "SkyboxRenderer.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "ImageLoader.h"

namespace
{
	// 샘플링 결과가 이미 선형인 포맷인지 판단한다.
	// - _SRGB 포맷: 하드웨어가 샘플링할 때 sRGB -> 선형으로 디코딩한다 (예: Sky.dds = BC7_UNORM_SRGB)
	// - float 포맷: .hdr처럼 원래 선형 값이다
	// 그 외 8bit/BC UNORM 포맷(.jpg/.png, 비-sRGB DDS)은 sRGB 인코딩 값이 그대로 나오므로 셰이더에서 디코딩한다.
	bool IsSampledAsLinear(DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			return true;
		default:
			return false;
		}
	}
}

bool FSkyboxRenderer::Init(const FString& PanoramaPath)
{
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/SkyboxShader.hlsl");
	if (!Shader)
	{
		HTR_LOG(Error, "[Skybox] shader not found");
		return false;
	}

	FImageData Image = ImageLoader::LoadAuto(PanoramaPath);
	if (!Image.IsValid())
	{
		HTR_LOG(Error, "[Skybox] panorama load failed: {}", PanoramaPath);
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Image.Width;
	Desc.Height = Image.Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = Image.Format;          // .hdr이면 R32G32B32A32_FLOAT
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	PanoramaTexture = RenderCommand::CreateTexture2D(Desc, Image);
	if (!PanoramaTexture) return false;

	bDecodeSRGBInShader = !IsSampledAsLinear(Image.Format);

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FSkyboxConstants));

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	// 전체 화면 삼각형이라 컬링 방향을 따질 필요가 없다.
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::Opaque;
	// 깊이 테스트/쓰기 모두 끈다. 배경을 먼저 깔고 물체가 그 위에 그려지게 한다.
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;

	HTR_LOG(Info, "[Skybox] loaded: {} ({}x{}, format {}, shader sRGB decode {})",
		PanoramaPath, Image.Width, Image.Height, static_cast<uint32>(Image.Format), bDecodeSRGBInShader ? "on" : "off");
	return true;
}

void FSkyboxRenderer::OnRender(const FMatrix& ViewProjection, const FVector& CameraPosition)
{
	if (!IsValid()) return;

	FSkyboxConstants Constants;
	// 업로드할 때 전치한다. HLSL은 mul(벡터, 행렬) 규약을 쓴다 (Renderer.cpp와 동일).
	Constants.InverseViewProjection = ViewProjection.Inverse();
	Constants.CameraPosition = CameraPosition;
	Constants.DecodeSRGB = bDecodeSRGBInShader ? 1.0f : 0.0f;

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, PanoramaTexture.get(), EShaderBindFlagBits::Pixel);
	// 경도(U)가 0<->1에서 감기므로 Clamp가 아니라 Wrap이어야 이음매가 안 보인다.
	RenderCommand::BindSamplerState(0, ESamplerState::LinearWrap, EShaderBindFlagBits::Pixel);

	// 정점 버퍼 없이 3개. VS가 SV_VertexID로 삼각형을 만든다.
	RenderCommand::Draw(3);
}                               
