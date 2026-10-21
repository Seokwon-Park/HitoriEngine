#include "EnginePCH.h"
#include "ImageLoader.h"
#include "RenderCommand.h"
#include "DDSTextureLoader11.h"

#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION   // 이미 다른 곳에 있으면 여기선 빼세요
#include "stb_image.h"

FImageData ImageLoader::Load(const FString& Path)
{
	FImageData Result;

	int Width = 0, Height = 0, Channels = 0;
	unsigned char* Pixels = stbi_load(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		OutputDebugStringA(("[Texture] stbi_load failed: " + Path + " (" + stbi_failure_reason() + ")\n").c_str());
		return FImageData();
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4;
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);

	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 4;
	Result.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	return Result;
}

FImageData ImageLoader::LoadHDR(const FString& Path)
{
	FImageData Result;

	stbi_set_flip_vertically_on_load(false);
	int Width = 0, Height = 0, Channels = 0;
	float* Pixels = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		HTR_LOG(Error, "[Image] HDR load failed: {} ({})", Path, stbi_failure_reason());
		return Result;
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4 * sizeof(float);
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);
	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 16;
	Result.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	return Result;
}

namespace
{
	// 포맷 한 단위의 바이트 수. BC 계열은 4x4 블록 하나의 크기를, 비압축은 픽셀 하나의 크기를 돌려준다.
	// 지원하지 않는 포맷은 0.
	uint32 GetDDSFormatUnitBytes(DXGI_FORMAT Format, bool& bOutBlockCompressed)
	{
		bOutBlockCompressed = false;
		switch (Format)
		{
		case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
			bOutBlockCompressed = true;
			return 8;
		case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
			bOutBlockCompressed = true;
			return 16;

		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return 16;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R32G32_FLOAT:
			return 8;
		case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM: case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return 4;
		case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_B5G6R5_UNORM: case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 2;
		case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_A8_UNORM:
			return 1;
		default:
			return 0;
		}
	}
}

// FRenderDevice::LoadDDSTexture와 같은 DirectXTK 로더를 쓰되, GPU용 리소스 대신 CPU가 읽을 수 있는
// STAGING 텍스처로 만든 뒤 최상위 밉(0번 서브리소스)만 FImageData로 복사한다.
// - 헤더 파싱·DX10 확장 헤더·레거시 픽셀 포맷 변환은 로더가 처리한다.
// - FImageData는 한 장짜리라 DDS에 들어 있던 나머지 밉은 버린다.
// - BC 압축 포맷은 RowPitch를 블록 단위로 채운다. 이 데이터로 텍스처를 만들 때 GENERATE_MIPS는 쓸 수 없다(BC는 RTV 불가).
FImageData ImageLoader::LoadDDS(const FString& Path)
{
	ID3D11Device* Device{ RenderCommand::GetDevice() };
	ID3D11DeviceContext* Context{ RenderCommand::GetContext() };
	if (!Device || !Context) {
		HTR_LOG(Error, "[Image] DDS load requires a render device: {}", Path);
		return FImageData();
	}

	const std::u8string UTF8Path{ Path.begin(), Path.end() };
	const std::wstring FilePath{ std::filesystem::path{ UTF8Path }.wstring() };

	ComPtr<ID3D11Resource> Resource{};
	const HRESULT LoadResult{ DirectX::CreateDDSTextureFromFileEx(
		Device,
		FilePath.c_str(),
		0,                          // maxsize: 제한 없음
		D3D11_USAGE_STAGING,
		0,                          // STAGING은 바인드 플래그를 가질 수 없다
		D3D11_CPU_ACCESS_READ,
		0,
		DirectX::DDS_LOADER_DEFAULT,
		Resource.GetAddressOf(),
		nullptr) };                 // STAGING은 SRV를 만들 수 없다
	if (FAILED(LoadResult)) {
		HTR_LOG(Error, "[Image] DDS load failed: {} (hr=0x{:08X})", Path, static_cast<uint32>(LoadResult));
		return FImageData();
	}

	ComPtr<ID3D11Texture2D> Texture{};
	if (FAILED(Resource.As(&Texture))) {
		HTR_LOG(Error, "[Image] DDS is not a 2D texture: {}", Path);
		return FImageData();
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Texture->GetDesc(&Desc);
	if (Desc.ArraySize != 1 || (Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0) {
		HTR_LOG(Error, "[Image] DDS arrays and cubemaps are not supported by FImageData: {}", Path);
		return FImageData();
	}

	bool bBlockCompressed{ false };
	const uint32 UnitBytes{ GetDDSFormatUnitBytes(Desc.Format, bBlockCompressed) };
	if (UnitBytes == 0) {
		HTR_LOG(Error, "[Image] Unsupported DDS format {}: {}", static_cast<uint32>(Desc.Format), Path);
		return FImageData();
	}

	// BC는 4x4 블록 한 줄이 한 행이다.
	const uint32 RowCount{ bBlockCompressed ? std::max(1u, (Desc.Height + 3) / 4) : Desc.Height };
	const uint32 RowBytes{ bBlockCompressed ? std::max(1u, (Desc.Width + 3) / 4) * UnitBytes : Desc.Width * UnitBytes };

	D3D11_MAPPED_SUBRESOURCE Mapped{};
	if (FAILED(Context->Map(Texture.Get(), 0, D3D11_MAP_READ, 0, &Mapped))) {
		HTR_LOG(Error, "[Image] DDS staging map failed: {}", Path);
		return FImageData();
	}

	FImageData Result;
	Result.Pixels.SetNum(static_cast<int32>(static_cast<size_t>(RowBytes) * RowCount));

	// 드라이버가 행 끝에 패딩을 붙일 수 있으므로(Mapped.RowPitch >= RowBytes) 행 단위로 복사해 촘촘하게 만든다.
	const uint8* Src{ static_cast<const uint8*>(Mapped.pData) };
	uint8* Dest{ Result.Pixels.GetData() };
	for (uint32 Row = 0; Row < RowCount; ++Row) {
		memcpy(Dest + static_cast<size_t>(Row) * RowBytes, Src + static_cast<size_t>(Row) * Mapped.RowPitch, RowBytes);
	}
	Context->Unmap(Texture.Get(), 0);

	Result.Width = Desc.Width;
	Result.Height = Desc.Height;
	Result.BytesPerPixel = bBlockCompressed ? 0 : UnitBytes;
	Result.RowPitch = RowBytes;
	Result.Format = Desc.Format;
	return Result;
}

FImageData ImageLoader::LoadAuto(const FString& Path)
{
	const fs::path Ext = fs::path(Path).extension();
	if (Ext == ".hdr" || Ext == ".HDR") return LoadHDR(Path);
	if (Ext == ".dds" || Ext == ".DDS") return LoadDDS(Path);
	return Load(Path);
}