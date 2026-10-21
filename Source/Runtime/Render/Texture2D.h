#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"
#include "Render/RenderDevice.h"
#include "Texture.h"
#include "ImageLoader.h"

class FTexture2D : public FTexture
{
public:
	// RowPitch가 0이면 InDesc.Format에서 계산한다.
	FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch = 0);
	// 로더가 포맷과 픽치까지 알고 있으므로 그대로 받는다.
	FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image);
	// For Swapchain
	FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc);
	~FTexture2D() = default;

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);
};

class UTexture2D : public URenderAsset
{
	DECLARE_CLASS(UTexture2D, URenderAsset)
	
	friend class UAssetManager;
public:
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth()  const { return Texture ? Texture->GetWidth() : 0; }
	uint32 GetHeight() const { return Texture ? Texture->GetHeight() : 0; }

	FTexture2D* GetResource() const { return Texture.get(); }
private:
	void SetResource(TUniquePtr<FTexture2D> InResource) { Texture = std::move(InResource); }

	TUniquePtr<FTexture2D> Texture;

};

