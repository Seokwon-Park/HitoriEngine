#include "EnginePCH.h"
#include "Buffer.h"

FBuffer::FBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, const void* InitialData)
{
	BufferSize = Desc.ByteWidth;

	HRESULT hr;
	if (InitialData)
	{
		D3D11_SUBRESOURCE_DATA Data;
		Data.pSysMem = InitialData;

		hr = Device->CreateBuffer(&Desc, &Data, Buffer.GetAddressOf());
	}
	else
	{
		hr = Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf());
	}
	if (FAILED(hr))
	{
		HTR_LOG(Warning, "Failed To Create Buffer!");
	}
}

FConstantBuffer::FConstantBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc)
	:FBuffer(Device, Desc)
{
}

FStructuredBuffer::FStructuredBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InCapacity)
	: FBuffer(Device, Desc), Capacity(InCapacity)
{
	if (!Buffer) return;
	D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
	ViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	ViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	ViewDesc.Buffer.NumElements = InCapacity;
	if (FAILED(Device->CreateShaderResourceView(Buffer.Get(), &ViewDesc, SRV.GetAddressOf())))
		HTR_LOG(Warning, "Failed to create structured buffer SRV");
}

FVertexBuffer::FVertexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InStride, const void* InitialData)
	:FBuffer(Device, Desc, InitialData)
{
	Stride = InStride;
}

FIndexBuffer::FIndexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 Count, const void* InitialData, DXGI_FORMAT InFormat)
	:FBuffer(Device, Desc, InitialData), Format(InFormat)
{
	IndexCount = Count;
}
