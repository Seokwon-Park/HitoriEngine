#include "EnginePCH.h"
#include "RenderDevice.h"

#include "Buffer.h"
#include "Texture2D.h"
#include "TextureCube.h"
#include "PipelineState.h"
#include "DDSTextureLoader11.h"

FRenderDevice::FRenderDevice()
{
	uint32 CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	// If the project is in a debug build, enable debugging via SDK Layers.
	CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		driverType,
		nullptr,
		CreationFlags,
		FeatureLevels,
		ARRAYSIZE(FeatureLevels),
		D3D11_SDK_VERSION,
		Device.GetAddressOf(),
		&FeatureLevel,
		DeviceContext.GetAddressOf());

	if (FAILED(hr))
	{
		HTR_LOG(Error, "Failed to Create D3D11Device & DeviceContext!");
	}

	// 오브젝트별 상수를 한 버퍼에 모아 두고 오프셋으로 바인딩하려면 D3D11.1 기능이 필요하다.
	if (SUCCEEDED(DeviceContext.As(&DeviceContext1)))
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS Options{};
		if (SUCCEEDED(Device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &Options, sizeof(Options))))
			bConstantBufferOffsetting = Options.ConstantBufferOffsetting == TRUE;
	}
	HTR_LOG(Info, "Constant buffer offsetting: {}", bConstantBufferOffsetting ? "on" : "off");

	ComPtr<IDXGIDevice1> DXGIDevice;
	if (SUCCEEDED(Device->QueryInterface(IID_PPV_ARGS(DXGIDevice.GetAddressOf()))))
	{
		DXGIDevice->SetMaximumFrameLatency(1);
	}
	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetParent(IID_PPV_ARGS(DXGIAdapter.GetAddressOf()));
	DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf()));

	CreateStates();
}

FRenderDevice::~FRenderDevice()
{
}


TUniquePtr<FVertexBuffer> FRenderDevice::CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;          // 나중에 규모 커지면 immutable
	Desc.ByteWidth = InSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride, InVertices);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateStaticIndexBuffer(const uint32* InIndices, uint32 IndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.ByteWidth = sizeof(uint32) * IndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	uint32 MaxIndex = 0;
	for (uint32 I = 0; I < IndexCount; ++I)
		MaxIndex = std::max(MaxIndex, InIndices[I]);
	if (MaxIndex <= 65535)
	{
		TArray<uint16> CompactIndices;
		CompactIndices.SetNum(IndexCount);
		for (uint32 I = 0; I < IndexCount; ++I)
			CompactIndices[I] = static_cast<uint16>(InIndices[I]);
		Desc.ByteWidth = sizeof(uint16) * IndexCount;
		return MakeUnique<FIndexBuffer>(Device.Get(), Desc, IndexCount, CompactIndices.GetData(), DXGI_FORMAT_R16_UINT);
	}
	return MakeUnique<FIndexBuffer>(Device.Get(), Desc, IndexCount, InIndices, DXGI_FORMAT_R32_UINT);
}

TUniquePtr<FVertexBuffer> FRenderDevice::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = MaxSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FIndexBuffer> Buffer = MakeUnique<FIndexBuffer>(Device.Get(), Desc, MaxIndexCount);

	return Buffer;
}

TUniquePtr<FConstantBuffer> FRenderDevice::CreateConstantBuffer(uint32 BufferSize)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = BufferSize;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FConstantBuffer> Buffer = MakeUnique<FConstantBuffer>(Device.Get(), Desc);

	return Buffer;
}

TUniquePtr<FStructuredBuffer> FRenderDevice::CreateDynamicStructuredBuffer(uint32 Stride, uint32 Count)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = Stride*Count;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;

	return MakeUnique<FStructuredBuffer>(Device.Get(), Desc, Count);
}

TUniquePtr<FStructuredBuffer> FRenderDevice::CreateRWStructuredBuffer(uint32 Stride, uint32 Count)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = Stride * Count;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;

	return MakeUnique<FStructuredBuffer>(Device.Get(), Desc, Count);
}

TUniquePtr<FTexture2D> FRenderDevice::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image)
{
	return MakeUnique<FTexture2D>(Device.Get(), Desc, Image);
}

TUniquePtr<FTexture2D> FRenderDevice::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	TUniquePtr<FTexture2D> Texture = MakeUnique<FTexture2D>(Device.Get(), Desc, InitialData);

	return Texture;
}

TUniquePtr<FTexture2D> FRenderDevice::LoadDDSTexture(const FString& Path) {
	const std::u8string UTF8Path{ Path.begin(), Path.end() };
	const std::wstring FilePath{ std::filesystem::path{ UTF8Path }.wstring() };
	ComPtr<ID3D11Resource> Resource{};
	// 메시·아이콘 텍스처는 모두 sRGB 인코딩 값 그대로(_UNORM) 올린다.
	// 색으로 쓰는 곳(UberLit 디퓨즈, 파티클)이 셰이더에서 직접 선형으로 바꾸고,
	// 노멀맵·톤 매핑 이후 오버레이(아이콘 등)는 인코딩 값 그대로 써야 하기 때문이다.
	// _SRGB DDS를 그대로 올리면 하드웨어 디코딩 + 셰이더 디코딩이 겹쳐 두 번 어두워진다.
	const HRESULT LoadResult{ DirectX::CreateDDSTextureFromFileEx(
		Device.Get(),
		FilePath.c_str(),
		0,                              // maxsize: 제한 없음
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0,                              // CPU 접근 없음
		0,
		DirectX::DDS_LOADER_IGNORE_SRGB,
		Resource.GetAddressOf(),
		nullptr) };
	if (FAILED(LoadResult)) {
		HTR_LOG(Error, "[Texture] DDS load failed: {} (hr=0x{:08X})", Path, static_cast<uint32>(LoadResult));
		return nullptr;
	}

	ComPtr<ID3D11Texture2D> Texture{};
	if (FAILED(Resource.As(&Texture))) 
	{
		HTR_LOG(Error, "[Texture] DDS is not a 2D texture: {}", Path);
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Texture->GetDesc(&Desc);
	if (Desc.ArraySize != 1 || (Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0) 
	{
		HTR_LOG(Error, "[Texture] DDS arrays and cubemaps are not supported by UTexture2D: {}", Path);
		return nullptr;
	}

	TUniquePtr<FTexture2D> LoadedTexture{ MakeUnique<FTexture2D>(Device.Get(), std::move(Resource), Desc) };
	if (!LoadedTexture->GetSRV()) 
	{
		return nullptr;
	}
	return LoadedTexture;
}

TUniquePtr<FTextureCube> FRenderDevice::CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas)
{
	TUniquePtr<FTextureCube> Texture = MakeUnique<FTextureCube>(Device.Get(), Desc, InitialDatas);

	return Texture;
}

TUniquePtr<FVertexShader> FRenderDevice::CreateVertexShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FVertexShader> VS = MakeUnique<FVertexShader>(Device.Get(), ByteCode);

	return VS;
}

TUniquePtr<FPixelShader> FRenderDevice::CreatePixelShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FPixelShader> PS = MakeUnique<FPixelShader>(Device.Get(), ByteCode);

	return PS;
}

TUniquePtr<FComputeShader> FRenderDevice::CreateComputeShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FComputeShader> CS = MakeUnique<FComputeShader>(Device.Get(), ByteCode);

	return CS;
}

//TUniquePtr<FShaderProgram> FRenderDevice::CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize)
//{
	//TUniquePtr<FShaderProgram> Shader = MakeUnique<FShaderProgram>();

	//ID3DBlob* VertexShaderCSO;
	//ID3DBlob* ErrorBlob;
	//HRESULT hr = D3DCompileFromFile(FileName, nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);

	//if (FAILED(hr))
	//{
	//	if (ErrorBlob)
	//	{
	//		OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
	//		ErrorBlob->Release();
	//	}
	//	assert(false && "Vertex shader compile failed");
	//	return nullptr; 
	//}

	//hr = Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, Shader->VertexShader.GetAddressOf());

	//ID3DBlob* PixelShaderCSO;
	//D3DCompileFromFile(FileName, nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, nullptr);
	//Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, Shader->PixelShader.GetAddressOf());

	//if (InLayoutSize > 0)
	//{
	//	hr = Device->CreateInputLayout(InLayoutDesc, InLayoutSize,
	//		VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &(Shader->InputLayout));
	//}

	//VertexShaderCSO->Release();
	//PixelShaderCSO->Release();

	//return Shader;
//}

void FRenderDevice::Shutdown()
{
	DeviceContext->ClearState();
	DeviceContext->Flush();
}

// RenderDevice.cpp
void FRenderDevice::CreateStates()
{
	// ---------- Rasterizer ----------
	{
		D3D11_RASTERIZER_DESC Desc = {};
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_BACK;
		Desc.FrontCounterClockwise = FALSE;
		Desc.DepthClipEnable = TRUE;

		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidBack].GetAddressOf());

		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidNone].GetAddressOf());

		Desc.CullMode = D3D11_CULL_FRONT;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidFront].GetAddressOf());

		Desc.FillMode = D3D11_FILL_WIREFRAME;
		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::Wireframe].GetAddressOf());

		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_BACK;
		Desc.DepthBias = 1000;
		Desc.SlopeScaledDepthBias = 2.0f;
		Desc.DepthBiasClamp = 0.0f;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::ShadowDepth].GetAddressOf());
	}

	// ---------- DepthStencil ----------
	{
		D3D11_DEPTH_STENCIL_DESC Desc = {};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		// Depth Prepass가 쓴 깊이와 같은 값을 본 패스가 다시 그려도 통과하도록 LESS_EQUAL
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		Desc.StencilEnable = FALSE;

		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Default].GetAddressOf());

		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::ReadOnly].GetAddressOf());

		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::PrepassReadOnly].GetAddressOf());
		Desc.DepthFunc = D3D11_COMPARISON_LESS;

		Desc.DepthEnable = FALSE;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Disabled].GetAddressOf());

		// 깊이 없이 선택 메시 영역의 스텐실을 올린다 (참조값 0 기준).
		Desc.StencilEnable = TRUE;
		Desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		Desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		Desc.BackFace = Desc.FrontFace;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::StencilMask].GetAddressOf());

		// 스텐실이 0인 픽셀(선택 메시 바깥)에만 그린다.
		Desc.StencilWriteMask = 0;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		Desc.BackFace = Desc.FrontFace;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::StencilOutline].GetAddressOf());
	}

	// ---------- Blend ----------
	{
		D3D11_BLEND_DESC Desc = {};
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::Opaque].GetAddressOf());

		Desc.RenderTarget[0].BlendEnable = TRUE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::AlphaBlend].GetAddressOf());

		// 라이트 기여분을 HDR 타깃에 더한다. 알파는 기존 값을 유지한다.
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::Additive].GetAddressOf());

		// 스텐실 마스크 패스처럼 색은 쓰지 않고 깊이·스텐실만 갱신할 때 쓴다.
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].RenderTargetWriteMask = 0;
		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::NoColorWrite].GetAddressOf());

		Desc.RenderTarget[0].BlendEnable = TRUE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;

		for (uint32 Channel = 0; Channel < 4; ++Channel)
		{
			Desc.RenderTarget[0].RenderTargetWriteMask = static_cast<UINT8>(1u << Channel);
			Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::ShadowMaskR + Channel].GetAddressOf());
		}
	}

	// ---------- Sampler ----------
	{
		D3D11_SAMPLER_DESC Desc = {};
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.MaxAnisotropy = 1;
		Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		Desc.MinLOD = 0.0f;
		Desc.MaxLOD = D3D11_FLOAT32_MAX;

		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::LinearClamp].GetAddressOf());

		Desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::LinearWrap].GetAddressOf());

		Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		Desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		Desc.BorderColor[0] = Desc.BorderColor[1] = Desc.BorderColor[2] = Desc.BorderColor[3] = 1.0f;
		Desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::ShadowCompare].GetAddressOf());
	}
}
