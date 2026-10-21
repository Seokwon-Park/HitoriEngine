#include "EnginePCH.h"
#include "RenderCommand.h"

#include "PipelineState.h"
#include "Buffer.h"
#include "Mesh.h"
#include "Shader.h"
#include "Texture2D.h"
#include "TextureCube.h"
#include "RenderingInfo.h"
#include <algorithm>

void RenderCommand::Init(FRenderDevice* InRenderDevice)
{
	RenderDevice = InRenderDevice;
}

TUniquePtr<FVertexBuffer> RenderCommand::CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	assert(RenderDevice);
	return RenderDevice->CreateStaticVertexBuffer(InVertices, InSize, Stride);
}

TUniquePtr<FVertexBuffer> RenderCommand::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	assert(RenderDevice);
	return RenderDevice->CreateDynamicVertexBuffer(MaxSize, Stride);
}

TUniquePtr<FIndexBuffer> RenderCommand::CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount)
{
	assert(RenderDevice);
	return RenderDevice->CreateStaticIndexBuffer(InIndices, MaxIndexCount);
}

TUniquePtr<FIndexBuffer> RenderCommand::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	assert(RenderDevice);
	return RenderDevice->CreateDynamicIndexBuffer(MaxIndexCount);
}

TUniquePtr<FConstantBuffer> RenderCommand::CreateConstantBuffer(uint32 BufferSize)
{
	assert(RenderDevice);
	return RenderDevice->CreateConstantBuffer(BufferSize);
}

TUniquePtr<FStructuredBuffer> RenderCommand::CreateDynamicStructuredBuffer(uint32 Capacity, uint32 Stride)
{
	assert(RenderDevice);
	if (!Capacity || !Stride || Capacity > UINT32_MAX / Stride)
		return nullptr;
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = Capacity * Stride;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;
	return std::make_unique<FStructuredBuffer>(RenderDevice->GetDevice(), Desc, Capacity);
}

TUniquePtr<FTexture2D> RenderCommand::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image)
{
	assert(RenderDevice);
	return RenderDevice->CreateTexture2D(Desc, Image);
}

TUniquePtr<FTexture2D> RenderCommand::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	assert(RenderDevice);
	return RenderDevice->CreateTexture2D(Desc, InitialData);
}

TUniquePtr<FTextureCube> RenderCommand::CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas)
{
	assert(RenderDevice);
	return RenderDevice->CreateTextureCube(Desc, InitialDatas);
}

TUniquePtr<FVertexShader> RenderCommand::CreateVertexShader(const FShaderByteCode& ByteCode)
{
	assert(RenderDevice);
	return RenderDevice->CreateVertexShader(ByteCode);
}

TUniquePtr<FPixelShader> RenderCommand::CreatePixelShader(const FShaderByteCode& ByteCode)
{
	assert(RenderDevice);
	return RenderDevice->CreatePixelShader(ByteCode);
}

void RenderCommand::BindPipelineState(const FPipelineState& PipelineState)
{
	BindShaderProgram(PipelineState.Shader);
	SetPrimitiveTopology(PipelineState.Topology);
	SetRasterizerState(PipelineState.RasterizerState);
	SetBlendState(PipelineState.BlendState);
	SetDepthStencilState(PipelineState.DepthStencilState);
}

void RenderCommand::BindShaderProgram(FShaderProgram* Shader)
{
	RenderDevice->GetContext()->VSSetShader(Shader->VertexShader->GetShader(), nullptr, 0);
	RenderDevice->GetContext()->PSSetShader(Shader->PixelShader->GetShader(), nullptr, 0);
	RenderDevice->GetContext()->IASetInputLayout(Shader->VertexShader->GetLayout());
}

void RenderCommand::BindMesh(UStaticMesh* Mesh, uint32 LODIndex)
{
	BindVertexBuffer(Mesh->GetVertexBuffer(LODIndex));
	BindIndexBuffer(Mesh->GetIndexBuffer(LODIndex));
}

void RenderCommand::Draw(uint32 VertexCount, uint32 StartIndexLocation)
{
	RenderDevice->GetContext()->Draw(VertexCount, StartIndexLocation);
}

void RenderCommand::DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation)
{
	RenderDevice->GetContext()->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}

void RenderCommand::DrawInstance(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation)
{
	//RenderDevice->GetContext()->DrawInstanced();
}

void RenderCommand::UpdateBufferData(FBuffer* InBuffer, const void* Data, uint32 DataSize)
{
	ID3D11Buffer* Buffer = InBuffer->GetBuffer();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	RenderDevice->GetContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	std::memcpy(MappedResource.pData, Data, DataSize);
	RenderDevice->GetContext()->Unmap(Buffer, 0);
}

void RenderCommand::BindVertexBuffer(FVertexBuffer* VertexBuffer)
{
	ID3D11Buffer* Buffer = VertexBuffer ? VertexBuffer->GetBuffer() : nullptr;

	uint32 Offset = 0;
	uint32 Stride = VertexBuffer ? VertexBuffer->GetStride() : 0;
	RenderDevice->GetContext()->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
}

void RenderCommand::BindIndexBuffer(FIndexBuffer* IndexBuffer)
{
	ID3D11Buffer* Buffer = IndexBuffer ? IndexBuffer->GetBuffer() : nullptr;

	uint32 Offset = 0;
	RenderDevice->GetContext()->IASetIndexBuffer(Buffer, IndexBuffer ? IndexBuffer->GetFormat() : DXGI_FORMAT_R32_UINT, Offset);
}

void RenderCommand::BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits)
{
	ID3D11Buffer* Buffer = ConstantBuffer ? ConstantBuffer->GetBuffer() : nullptr;
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
		RenderDevice->GetContext()->VSSetConstantBuffers(Slot, 1, &Buffer);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
		RenderDevice->GetContext()->PSSetConstantBuffers(Slot, 1, &Buffer);

}

bool RenderCommand::SupportsConstantBufferOffsets()
{
	return RenderDevice->GetContext1() != nullptr;
}

void RenderCommand::BindConstantBufferRange(uint32 Slot, FConstantBuffer* ConstantBuffer, uint32 FirstConstant, uint32 NumConstants, EShaderBindFlagBits FlagBits)
{
	ID3D11DeviceContext1* Context = RenderDevice->GetContext1();
	ID3D11Buffer* Buffer = ConstantBuffer->GetBuffer();
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
		Context->VSSetConstantBuffers1(Slot, 1, &Buffer, &FirstConstant, &NumConstants);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
		Context->PSSetConstantBuffers1(Slot, 1, &Buffer, &FirstConstant, &NumConstants);
}

void* RenderCommand::MapWriteDiscard(FBuffer* InBuffer)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource{};
	if (FAILED(RenderDevice->GetContext()->Map(InBuffer->GetBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
		return nullptr;
	return MappedResource.pData;
}

void RenderCommand::Unmap(FBuffer* InBuffer)
{
	RenderDevice->GetContext()->Unmap(InBuffer->GetBuffer(), 0);
}

void RenderCommand::BindShaderResource(uint32 Slot, FTexture2D* Texture2D, EShaderBindFlagBits FlagBits)
{
	ID3D11ShaderResourceView* SRV = Texture2D ? Texture2D->GetSRV() : nullptr;
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
	{
		RenderDevice->GetContext()->VSSetShaderResources(Slot, 1, &SRV);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
	{
		RenderDevice->GetContext()->PSSetShaderResources(Slot, 1, &SRV);
	}
}

void RenderCommand::BindShaderResource(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits)
{
	BindShaderResource(Slot, Texture2D ? Texture2D->GetResource() : nullptr, FlagBits);
}

void RenderCommand::BindShaderResource(uint32 Slot, const FTextureCube* TextureCube, EShaderBindFlagBits FlagBits)
{
	ID3D11ShaderResourceView* SRV = TextureCube ? TextureCube->GetSRV() : nullptr;
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
		RenderDevice->GetContext()->VSSetShaderResources(Slot, 1, &SRV);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
		RenderDevice->GetContext()->PSSetShaderResources(Slot, 1, &SRV);
}

void RenderCommand::BindShaderResource(uint32 Slot, FStructuredBuffer* StructuredBuffer, EShaderBindFlagBits FlagBits)
{
	ID3D11ShaderResourceView* SRV = StructuredBuffer ? StructuredBuffer->GetSRV() : nullptr;
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
		RenderDevice->GetContext()->VSSetShaderResources(Slot, 1, &SRV);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
		RenderDevice->GetContext()->PSSetShaderResources(Slot, 1, &SRV);
}


void RenderCommand::UnbindShaderResources(uint32 FirstSlot, uint32 Count, EShaderBindFlagBits FlagBits)
{
    constexpr uint32 MaxSlots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    if (FirstSlot >= MaxSlots || Count == 0) return;
    Count = std::min(Count, MaxSlots - FirstSlot);
    ID3D11ShaderResourceView* NullResources[MaxSlots]{};
    if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
        RenderDevice->GetContext()->VSSetShaderResources(FirstSlot, Count, NullResources);
    if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
        RenderDevice->GetContext()->PSSetShaderResources(FirstSlot, Count, NullResources);
}

// 텍스처 기반 렌더 패스 시작
void RenderCommand::BeginRenderPass(FTexture2D* ColorTarget, FTexture2D* DepthTarget, uint32 Width, uint32 Height)
{
	ID3D11RenderTargetView* RTV = ColorTarget ? ColorTarget->GetRTV() : nullptr;
	ID3D11DepthStencilView* DSV = DepthTarget ? DepthTarget->GetDSV() : nullptr;

	if (RTV)
	{
		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		RenderDevice->GetContext()->ClearRenderTargetView(RTV, ClearColor);
	}
	if (DSV)
	{
		RenderDevice->GetContext()->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	RenderDevice->GetContext()->OMSetRenderTargets(RTV ? 1 : 0, RTV ? &RTV : nullptr, DSV);
	SetViewport(0, 0, Width, Height);
}

void RenderCommand::BindRenderPassNoClear(FTexture2D* ColorTarget, FTexture2D* DepthTarget, uint32 Width, uint32 Height)
{
	ID3D11RenderTargetView* RTV = ColorTarget ? ColorTarget->GetRTV() : nullptr;
	ID3D11DepthStencilView* DSV = DepthTarget ? DepthTarget->GetDSV() : nullptr;
	RenderDevice->GetContext()->OMSetRenderTargets(RTV ? 1 : 0, RTV ? &RTV : nullptr, DSV);
	SetViewport(0, 0, Width, Height);
}

// 렌더 패스 종료
void RenderCommand::EndRenderPass()
{
	RenderDevice->GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void RenderCommand::ClearDepthStencil(FTexture2D* DepthStencilTexture, float Depth, uint8 Stencil)
{
	if (DepthStencilTexture == nullptr)
	{
		return;
	}

	RenderDevice->GetContext()->ClearDepthStencilView(DepthStencilTexture->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
}

void RenderCommand::SetRenderTargets(FTexture2D* ColorTarget, FTexture2D* DepthTarget)
{
	SetRenderTargets(ColorTarget ? 1u : 0u, ColorTarget ? &ColorTarget : nullptr, DepthTarget);
}

void RenderCommand::SetRenderTargets(uint32 ColorTargetCount, FTexture2D* const* ColorTargets,
	FTexture2D* DepthTarget)
{
	assert(ColorTargetCount <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
	ID3D11RenderTargetView* RTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	for (uint32 Index = 0; Index < ColorTargetCount; ++Index)
		RTVs[Index] = ColorTargets[Index] ? ColorTargets[Index]->GetRTV() : nullptr;
	RenderDevice->GetContext()->OMSetRenderTargets(ColorTargetCount,
		ColorTargetCount ? RTVs : nullptr, DepthTarget ? DepthTarget->GetDSV() : nullptr);
}

void RenderCommand::ClearRenderTarget(FTexture2D* ColorTarget, const float (&Color)[4])
{
	RenderDevice->GetContext()->ClearRenderTargetView(ColorTarget->GetRTV(), Color);
}


void RenderCommand::SetViewport(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight)
{
	D3D11_VIEWPORT Viewport;
	Viewport.TopLeftX = (float)InX;
	Viewport.TopLeftY = (float)InY;
	Viewport.Width = (float)InWidth;
	Viewport.Height = (float)InHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	RenderDevice->GetContext()->RSSetViewports(1, &Viewport);
}

void RenderCommand::SetRasterizerState(ERasterizerState State)
{
	RenderDevice->GetContext()->RSSetState(RenderDevice->GetRasterizerState(State));
}

void RenderCommand::SetBlendState(EBlendState State)
{
	RenderDevice->GetContext()->OMSetBlendState(RenderDevice->GetBlendState(State), nullptr, 0xffffffff);
}

void RenderCommand::SetDepthStencilState(EDepthStencilState State)
{
	RenderDevice->GetContext()->OMSetDepthStencilState(RenderDevice->GetDepthStencilState(State), 0);
}

void RenderCommand::BindSamplerState(uint32 Slot, ESamplerState SamplerState, EShaderBindFlagBits FlagBits)
{
	ID3D11SamplerState* Sampler = RenderDevice->GetSamplerState(SamplerState);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
	{
		RenderDevice->GetContext()->VSSetSamplers(Slot, 1, &Sampler);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
	{
		RenderDevice->GetContext()->PSSetSamplers(Slot, 1, &Sampler);
	}
}

