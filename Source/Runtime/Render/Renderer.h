#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "MeshPassProcessor.h"
#include <span>

class FRenderer
{
public:
	bool Init();

	// 패스의 의미를 해석하지 않고 준비된 GPU 상태와 드로우만 제출한다.
	void UploadViewMatrix(const FMatrix& ViewProjection);
	void DrawMeshes(std::span<const FMeshDrawBatch> Batches);
	void DrawFullscreen(const FPipelineState& Pipeline,
		std::span<FTexture2D* const> Textures);
	void BindPixelConstant(uint32 Slot, FConstantBuffer* Buffer);
	void UpdateConstant(FConstantBuffer* Buffer, const void* Data, uint32 Size);
	void BindConstant(uint32 Slot, FConstantBuffer* Buffer, EShaderBindFlagBits Stages);
	void BindStructured(uint32 Slot, FStructuredBuffer* Buffer, EShaderBindFlagBits Stages);
	void BindCube(uint32 Slot, const FTextureCube* Texture, EShaderBindFlagBits Stages);
	void BindSampler(uint32 Slot, ESamplerState Sampler, EShaderBindFlagBits Stages);
	void UnbindResources(uint32 FirstSlot, uint32 Count, EShaderBindFlagBits Stages);
	void BindRawPixelConstant(uint32 Slot, ID3D11Buffer* Buffer);
	void BindRawPixelResources(uint32 FirstSlot,
		std::span<ID3D11ShaderResourceView* const> Resources);
	void DispatchCompute(ID3D11ComputeShader* Shader, ID3D11Buffer* Constants,
		uint32 SRVSlot, ID3D11ShaderResourceView* SRV,
		std::span<ID3D11UnorderedAccessView* const> UAVs,
		uint32 GroupsX, uint32 GroupsY, uint32 GroupsZ,
		ID3D11SamplerState* Sampler = nullptr);
	void DispatchCompute(ID3D11ComputeShader* Shader, ID3D11Buffer* Constants,
		uint32 SRVSlot, std::span<ID3D11ShaderResourceView* const> SRVs,
		std::span<ID3D11UnorderedAccessView* const> UAVs,
		uint32 GroupsX, uint32 GroupsY, uint32 GroupsZ,
		ID3D11SamplerState* Sampler = nullptr);
	void ClearUnorderedAccess(ID3D11UnorderedAccessView* UAV, uint32 Value = 0);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 측정 대상 드로우를 동일한 GPU 상태로 다시 제출한다.
	FOcclusionMeasureResult MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

	FTexture2D* GetShadowAtlas(uint32 Size);

private:
	TUniquePtr<FTexture2D> ShadowAtlas;

	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void BindBatch(const FMeshDrawBatch& Batch);
	void BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item);
	TUniquePtr<FConstantBuffer> ObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> MaterialCB;
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;
	FGPUOcclusion GPUOcclusion;
};
