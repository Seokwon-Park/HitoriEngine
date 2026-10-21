#include "EnginePCH.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/ImageLoader.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"
#include "Render/Renderer.h"

namespace
{
	constexpr uint32 EnvironmentSize = 128, IrradianceSize = 32, IntegrationSamples = 512;
	struct FSkyConstants
	{
		uint32 OutputSize, PanoramaHeight, bSourceIsLinear, SampleCount;
	};
	static_assert(sizeof(FSkyConstants) == 16);

	bool CompileCompute(ID3D11Device* Device, const char* Entry, ComPtr<ID3D11ComputeShader>& Shader)
	{
		ComPtr<ID3DBlob> Code, Errors;
		HRESULT Hr = D3DCompileFromFile(L"Resources/Shader/SkyLightConvolutionCS.hlsli",
			nullptr, nullptr, Entry, "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
			Code.GetAddressOf(), Errors.GetAddressOf());
		if (FAILED(Hr))
		{
			if (Errors) HTR_LOG(Error, "[SkyLight] {}: {}", Entry,
				static_cast<const char*>(Errors->GetBufferPointer()));
			else HTR_LOG(Error, "[SkyLight] {} compile failed: 0x{:08X}", Entry, uint32(Hr));
			return false;
		}
		return SUCCEEDED(Device->CreateComputeShader(Code->GetBufferPointer(),
			Code->GetBufferSize(), nullptr, Shader.GetAddressOf()));
	}

	TUniquePtr<FTextureCube> CreateCube(uint32 Size)
	{
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Desc.Height = Size;
		Desc.MipLevels = 1;
		Desc.ArraySize = FTextureCube::FaceCount;
		Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		auto Cube = RenderCommand::CreateTextureCube(Desc, {});
		return Cube && Cube->GetSRV() && Cube->GetUAV() ? std::move(Cube) : nullptr;
	}
}

bool FSkyLightEnvironment::Prepare(const FString& InSourcePath)
{
	FImageData Image = ImageLoader::LoadAuto(InSourcePath);
	if (!Image.IsValid() || (Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		Image.Format != DXGI_FORMAT_R32G32B32A32_FLOAT))
	{
		HTR_LOG(Warning, "[SkyLight] unsupported panorama: {}", InSourcePath);
		return false;
	}

	D3D11_TEXTURE2D_DESC PanoramaDesc{};
	PanoramaDesc.Width = Image.Width;
	PanoramaDesc.Height = Image.Height;
	PanoramaDesc.MipLevels = Image.MipLevels;
	PanoramaDesc.ArraySize = 1;
	PanoramaDesc.Format = Image.Format;
	PanoramaDesc.SampleDesc.Count = 1;
	PanoramaDesc.Usage = D3D11_USAGE_IMMUTABLE;
	PanoramaDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	auto NewPanorama = RenderCommand::CreateTexture2D(PanoramaDesc, Image);
	auto NewEnvironment = CreateCube(EnvironmentSize);
	auto NewIrradiance = CreateCube(IrradianceSize);
	if (!NewPanorama || !NewPanorama->GetSRV() || !NewEnvironment || !NewIrradiance)
	{
		HTR_LOG(Warning, "[SkyLight] GPU texture creation failed: {}", InSourcePath);
		return false;
	}

	ID3D11Device* Device = RenderCommand::GetDevice();
	ComPtr<ID3D11ComputeShader> NewEnvironmentCS, NewIrradianceCS;
	if (!CompileCompute(Device, "BuildEnvironmentCube", NewEnvironmentCS) ||
		!CompileCompute(Device, "BuildIrradianceCube", NewIrradianceCS)) return false;

	D3D11_BUFFER_DESC CBDesc{};
	CBDesc.ByteWidth = sizeof(FSkyConstants);
	CBDesc.Usage = D3D11_USAGE_DEFAULT;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ComPtr<ID3D11Buffer> NewConstantsCB;
	if (FAILED(Device->CreateBuffer(&CBDesc, nullptr, NewConstantsCB.GetAddressOf()))) return false;

	D3D11_SAMPLER_DESC SamplerDesc{};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	ComPtr<ID3D11SamplerState> NewSampler;
	if (FAILED(Device->CreateSamplerState(&SamplerDesc, NewSampler.GetAddressOf()))) return false;
	Panorama = std::move(NewPanorama);
	EnvironmentCube = std::move(NewEnvironment);
	IrradianceCube = std::move(NewIrradiance);
	EnvironmentCS = std::move(NewEnvironmentCS);
	IrradianceCS = std::move(NewIrradianceCS);
	ConstantsCB = std::move(NewConstantsCB);
	Sampler = std::move(NewSampler);
	PanoramaHeight = Image.Height;
	bSourceIsLinear = Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM;
	SourcePath = InSourcePath;
	return true;
}

void FSkyLightEnvironment::DispatchEnvironment(FRenderer* Renderer)
{
	const FSkyConstants Constants{ EnvironmentSize, PanoramaHeight, bSourceIsLinear ? 1u : 0u,
		IntegrationSamples };
	RenderCommand::GetContext()->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &Constants, 0, 0);
	ID3D11UnorderedAccessView* UAVs[] = { EnvironmentCube->GetUAV() };
	Renderer->DispatchCompute(EnvironmentCS.Get(), ConstantsCB.Get(), 0, Panorama->GetSRV(),
		UAVs, EnvironmentSize / 8, EnvironmentSize / 8, FTextureCube::FaceCount, Sampler.Get());
}

void FSkyLightEnvironment::DispatchIrradiance(FRenderer* Renderer)
{
	const FSkyConstants Constants{ IrradianceSize, PanoramaHeight, 1u, IntegrationSamples };
	RenderCommand::GetContext()->UpdateSubresource(ConstantsCB.Get(), 0, nullptr, &Constants, 0, 0);
	ID3D11UnorderedAccessView* UAVs[] = { IrradianceCube->GetUAV() };
	Renderer->DispatchCompute(IrradianceCS.Get(), ConstantsCB.Get(), 1, EnvironmentCube->GetSRV(),
		UAVs, IrradianceSize / 8, IrradianceSize / 8, FTextureCube::FaceCount, Sampler.Get());
}

void FSkyLightEnvironment::Complete()
{
	bReady = true;
	Panorama.reset();
	EnvironmentCS.Reset();
	IrradianceCS.Reset();
	ConstantsCB.Reset();
	Sampler.Reset();
	HTR_LOG(Info, "[SkyLight] GPU environment {}x{}, irradiance {}x{}: {}",
		EnvironmentSize, EnvironmentSize, IrradianceSize, IrradianceSize, SourcePath);
}

ID3D11Resource* FSkyLightEnvironment::GetPanoramaResource() const { return Panorama ? Panorama->GetRawPtr() : nullptr; }
ID3D11Resource* FSkyLightEnvironment::GetEnvironmentResource() const { return EnvironmentCube ? EnvironmentCube->GetRawPtr() : nullptr; }
ID3D11Resource* FSkyLightEnvironment::GetIrradianceResource() const { return IrradianceCube ? IrradianceCube->GetRawPtr() : nullptr; }
