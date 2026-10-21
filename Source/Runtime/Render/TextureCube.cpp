#include "EnginePCH.h"
#include "TextureCube.h"

FTextureCube::FTextureCube(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const TArray<const void*>& InitialDatas)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::TextureCube;

	TArray<D3D11_SUBRESOURCE_DATA> SubDatas;
	if (!InitialDatas.IsEmpty())
	{
		SubDatas.SetNum(InitialDatas.size());
		for (int i = 0; i < InitialDatas.size(); i++)
		{
			SubDatas[i].pSysMem = InitialDatas[i];
			SubDatas[i].SysMemPitch = InDesc.Width * FormatToBytes(InDesc.Format);
			SubDatas[i].SysMemSlicePitch = 0;
		}
	}

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitialDatas.IsEmpty() ? nullptr : SubDatas.GetData(), (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr))
	{
		HTR_LOG(Error, "[TextureCube] CreateTexture2D failed (hr=0x{:08X}, {}x{})", (uint32)hr, InDesc.Width, InDesc.Height);
		return;
	}

	CreateViews(Device, InDesc);
}

void FTextureCube::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc)
{
	HRESULT hr;

	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		// 기본 SRV는 큐브 전체. 차원을 명시하지 않으면 Texture2DArray로 잡혀서
		// 셰이더의 TextureCube와 어긋난다.
		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = InDesc.Format;
		SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		SrvDesc.TextureCube.MostDetailedMip = 0;
		SrvDesc.TextureCube.MipLevels = InDesc.MipLevels;

		hr = Device->CreateShaderResourceView(Texture.Get(), &SrvDesc, SRV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[TextureCube] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)hr);
	}

	if (InDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		// 면마다 RTV를 하나씩. 배열 슬라이스 하나만 보는 뷰다.
		for (uint32 Face = 0; Face < FaceCount; ++Face)
		{
			D3D11_RENDER_TARGET_VIEW_DESC RtvDesc{};
			RtvDesc.Format = InDesc.Format;
			RtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			RtvDesc.Texture2DArray.MipSlice = 0;
			RtvDesc.Texture2DArray.FirstArraySlice = Face;
			RtvDesc.Texture2DArray.ArraySize = 1;

			hr = Device->CreateRenderTargetView(Texture.Get(), &RtvDesc, FaceRTVs[Face].GetAddressOf());
			if (FAILED(hr))
				HTR_LOG(Error, "[TextureCube] CreateRenderTargetView failed (face={}, hr=0x{:08X})", Face, (uint32)hr);
		}
	}

	if (InDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc{};
		UavDesc.Format = InDesc.Format;
		UavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		UavDesc.Texture2DArray.MipSlice = 0;
		UavDesc.Texture2DArray.FirstArraySlice = 0;
		UavDesc.Texture2DArray.ArraySize = FaceCount;
		hr = Device->CreateUnorderedAccessView(Texture.Get(), &UavDesc, UAV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[TextureCube] CreateUnorderedAccessView failed (hr=0x{:08X})", (uint32)hr);
	}
}
