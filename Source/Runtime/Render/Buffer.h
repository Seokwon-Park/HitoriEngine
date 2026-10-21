#pragma once

class FBuffer
{
public:
	FBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, const void* InitialData = nullptr);
	virtual ~FBuffer() = default;

	inline uint32 GetBufferSize() const { return BufferSize; }
	inline ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

protected:
	uint32 BufferSize = 0;
	ComPtr<ID3D11Buffer> Buffer = nullptr;
private:
};

class FVertexBuffer : public FBuffer
{
public:
	//static
	FVertexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InStride, const void* InitialData = nullptr);
	~FVertexBuffer() override = default;

	inline uint32 GetStride() const { return Stride; }
private:
	uint32 Stride = 0;
};


class FIndexBuffer : public FBuffer
{
public:
	FIndexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 Count, const void* InitialData = nullptr, DXGI_FORMAT InFormat = DXGI_FORMAT_R32_UINT);
	~FIndexBuffer() override = default;

	inline uint32 GetIndexCount() const { return IndexCount; }
	inline DXGI_FORMAT GetFormat() const { return Format; }
private:
	uint32 IndexCount;
	DXGI_FORMAT Format;

};


class FConstantBuffer : public FBuffer
{
public:
	FConstantBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc);
	~FConstantBuffer() override = default;

private:

};

class FStructuredBuffer : public FBuffer
{
public:
	FStructuredBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InCapacity);
	ID3D11ShaderResourceView* GetSRV() const { return SRV.Get(); }
	uint32 GetCapacity() const { return Capacity; }

private:
	uint32 Capacity = 0;
	ComPtr<ID3D11ShaderResourceView> SRV;
};
