#pragma once

#include "RenderDevice.h"

class FBuffer;
class FStructuredBuffer;
class UStaticMesh;
class UTexture2D;
class FTextureCube;
class FShaderProgram;
class FVertexShader;
class FPixelShader;

enum class EShaderBindFlagBits : uint32
{
	None = 0,
	Vertex = 1,
	Geometry = 1 << 2,
	Domain = 1 << 3,
	Hull = 1 << 4,
	Pixel = 1 << 5,
	Compute = 1 << 6
};

DEFINE_ENUM_OPERATORS(EShaderBindFlagBits);

class RenderCommand
{
public:
	static void Init(FRenderDevice* InRenderDevice);

	static TUniquePtr<FVertexBuffer> CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	static TUniquePtr<FVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);

	static TUniquePtr<FIndexBuffer> CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);
	static TUniquePtr<FIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	static TUniquePtr<FConstantBuffer> CreateConstantBuffer(uint32 BufferSize);
	static TUniquePtr<FStructuredBuffer> CreateDynamicStructuredBuffer(uint32 Capacity, uint32 Stride);

	static TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
	static TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image);
	static TUniquePtr<FTextureCube> CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas);

	static TUniquePtr<FVertexShader> CreateVertexShader(const FShaderByteCode& ByteCode);
	static TUniquePtr<FPixelShader> CreatePixelShader(const FShaderByteCode& ByteCode);

	static void BindPipelineState(const FPipelineState& PipelineState);

	static void BindMesh(UStaticMesh* Mesh, uint32 LODIndex = 0);

	static void BindShaderProgram(FShaderProgram* Shader);

	static void Draw(uint32 VertexCount, uint32 StartIndexLocation = 0);
	static void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0);
	static void DrawInstance(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0);

	template <typename T>
	static void UpdateBufferData(FBuffer* InBuffer, T* Data)
	{
		UpdateBufferData(InBuffer, Data, sizeof(T));
	}
	static void UpdateBufferData(FBuffer* InBuffer, const void* Data, uint32 DataSize);

	static void BindVertexBuffer(FVertexBuffer* VertexBuffer);
	static void BindIndexBuffer(FIndexBuffer* IndexBuffer);
	// null 버퍼를 전달하면 해당 슬롯의 바인딩을 해제한다.
	static void BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits);

	// 상수 버퍼의 일부 구간만 바인딩한다. FirstConstant·NumConstants는 16바이트 단위이며 16의 배수여야 한다.
	static bool SupportsConstantBufferOffsets();
	static void BindConstantBufferRange(uint32 Slot, FConstantBuffer* ConstantBuffer, uint32 FirstConstant, uint32 NumConstants, EShaderBindFlagBits FlagBits);

	// 한 번의 Map으로 버퍼 전체를 채울 때 쓴다. 반환 포인터에는 순차적으로 쓰기만 해야 한다.
	static void* MapWriteDiscard(FBuffer* InBuffer);
	static void Unmap(FBuffer* InBuffer);
	static void BindShaderResource(uint32 Slot, FTexture2D* Texture2D, EShaderBindFlagBits FlagBits);
	static void BindShaderResource(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits);
	static void BindShaderResource(uint32 Slot, const FTextureCube* TextureCube, EShaderBindFlagBits FlagBits);
	static void BindShaderResource(uint32 Slot, FStructuredBuffer* StructuredBuffer, EShaderBindFlagBits FlagBits);
	static void UnbindShaderResources(uint32 FirstSlot, uint32 Count, EShaderBindFlagBits FlagBits);

	static void BeginRenderPass(FTexture2D* ColorTarget, FTexture2D* DepthTarget, uint32 Width, uint32 Height);
	static void BindRenderPassNoClear(FTexture2D* ColorTarget, FTexture2D* DepthTarget, uint32 Width, uint32 Height);
	static void EndRenderPass();
	static void ClearDepthStencil(FTexture2D* DepthStencilTexture, float Depth = 1.0f, uint8 Stencil = 0);
	static void SetRenderTargets(FTexture2D* ColorTarget, FTexture2D* DepthTarget);
	static void SetRenderTargets(uint32 ColorTargetCount, FTexture2D* const* ColorTargets, FTexture2D* DepthTarget);
	static void ClearRenderTarget(FTexture2D* ColorTarget, const float (&Color)[4]);

	static void SetViewport(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight);

	static void SetRasterizerState(ERasterizerState State);
	static void SetBlendState(EBlendState State);
	static void SetDepthStencilState(EDepthStencilState State);
	static void BindSamplerState(uint32 Slot, ESamplerState SamplerState, EShaderBindFlagBits FlagBits);

	inline static void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
	{
		RenderDevice->GetContext()->IASetPrimitiveTopology(Topology);
	}

	// 래퍼가 없는 D3D 기능(쿼리 등)을 디버그·측정 코드에서 직접 쓸 때만 사용한다.
	inline static ID3D11Device* GetDevice() { return RenderDevice->GetDevice(); }
	inline static ID3D11DeviceContext* GetContext() { return RenderDevice->GetContext(); }


private:
	inline static FRenderDevice* RenderDevice = nullptr;
};
