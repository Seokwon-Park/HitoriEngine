#pragma once

// 압축되지 않은 포맷의 픽셀당 바이트 수.
// 압축(BC*)·비디오 포맷은 블록 단위라 픽셀당 바이트가 없으므로 0을 반환한다.
uint32 FormatToBytes(DXGI_FORMAT Format);

enum class ETextureDimension : uint8
{
	Texture1D,
	Texture2D,
	Texture2DArray,
	TextureCube,
	Texture3D,
};

class FTexture
{
public:
	FTexture();
	virtual ~FTexture() = default;

	uint32 GetWidth()  const { return Width; }
	uint32 GetHeight() const { return Height; }
	uint32 GetDepth()  const { return Depth; }

	DXGI_FORMAT       GetFormat()    const { return Format; }
	ETextureDimension GetDimension() const { return Dimension; }

	ID3D11Resource* GetRawPtr() const { return Texture.Get(); }
	ID3D11ShaderResourceView* GetSRV() const { return SRV.Get(); }
	ID3D11RenderTargetView* GetRTV() const { return RTV.Get(); }
	ID3D11DepthStencilView* GetDSV() const { return DSV.Get(); }
protected:
	// 크기는 API 구조체가 아니라 숫자로 들고 있는다.
	// Texture3D가 D3D11_TEXTURE3D_DESC로 와도 여기는 그대로 쓴다.
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 Depth = 1;

	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	ETextureDimension Dimension = ETextureDimension::Texture2D;

	ComPtr<ID3D11Resource> Texture;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11RenderTargetView> RTV;
	ComPtr<ID3D11DepthStencilView> DSV;

};
