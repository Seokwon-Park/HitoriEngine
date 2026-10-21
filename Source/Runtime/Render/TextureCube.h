#pragma once

#include "Texture.h"

class FTextureCube : public FTexture
{
public:
	static constexpr uint32 FaceCount = 6;

	FTextureCube(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const TArray<const void*>& InitialDatas);
	~FTextureCube() = default;

	// 면 단위 렌더 타겟. Equirectangular -> Cubemap 변환에서 면마다 하나씩 쓴다.
	// 면 순서는 D3D 규약: +X, -X, +Y, -Y, +Z, -Z
	ID3D11RenderTargetView* GetFaceRTV(uint32 Face) const
	{
		return Face < FaceCount ? FaceRTVs[Face].Get() : nullptr;
	}
	ID3D11UnorderedAccessView* GetUAV() const { return UAV.Get(); }

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);

	ComPtr<ID3D11RenderTargetView> FaceRTVs[FaceCount];
	ComPtr<ID3D11UnorderedAccessView> UAV;
};
