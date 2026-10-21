#include "EnginePCH.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace
{
	constexpr uint32 PerObjectSlotConstants = 16;
	constexpr uint32 MinPerObjectSlots = 1024;
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	ObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	MaterialCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardMaterialConstants));
	if (!ObjectCB || !ViewCB || !MaterialCB) return false;
	GPUOcclusion.Init();
	return true;
}

void FRenderer::UploadViewMatrix(const FMatrix& ViewProjection)
{
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection, sizeof(ViewProjection));
}

void FRenderer::BindPixelConstant(uint32 Slot, FConstantBuffer* Buffer)
{
	RenderCommand::BindConstantBuffer(Slot, Buffer, EShaderBindFlagBits::Pixel);
}

void FRenderer::UpdateConstant(FConstantBuffer* Buffer, const void* Data, uint32 Size)
{
	RenderCommand::UpdateBufferData(Buffer, Data, Size);
}

void FRenderer::BindConstant(uint32 Slot, FConstantBuffer* Buffer, EShaderBindFlagBits Stages)
{
	RenderCommand::BindConstantBuffer(Slot, Buffer, Stages);
}

void FRenderer::BindStructured(uint32 Slot, FStructuredBuffer* Buffer, EShaderBindFlagBits Stages)
{
	RenderCommand::BindShaderResource(Slot, Buffer, Stages);
}

void FRenderer::BindCube(uint32 Slot, const FTextureCube* Texture, EShaderBindFlagBits Stages)
{
	RenderCommand::BindShaderResource(Slot, Texture, Stages);
}

void FRenderer::BindSampler(uint32 Slot, ESamplerState Sampler, EShaderBindFlagBits Stages)
{
	RenderCommand::BindSamplerState(Slot, Sampler, Stages);
}

void FRenderer::UnbindResources(uint32 FirstSlot, uint32 Count, EShaderBindFlagBits Stages)
{
	RenderCommand::UnbindShaderResources(FirstSlot, Count, Stages);
}

void FRenderer::BindRawPixelConstant(uint32 Slot, ID3D11Buffer* Buffer)
{
	RenderCommand::GetContext()->PSSetConstantBuffers(Slot, 1, &Buffer);
}

void FRenderer::BindRawPixelResources(uint32 FirstSlot,
	std::span<ID3D11ShaderResourceView* const> Resources)
{
	RenderCommand::GetContext()->PSSetShaderResources(FirstSlot,
		static_cast<UINT>(Resources.size()), Resources.data());
}

void FRenderer::DispatchCompute(ID3D11ComputeShader* Shader, ID3D11Buffer* Constants,
	uint32 SRVSlot, ID3D11ShaderResourceView* SRV,
	std::span<ID3D11UnorderedAccessView* const> UAVs,
	uint32 GroupsX, uint32 GroupsY, uint32 GroupsZ, ID3D11SamplerState* Sampler)
{
	const std::array<ID3D11ShaderResourceView*, 1> SRVs = { SRV };
	DispatchCompute(Shader, Constants, SRVSlot, SRVs, UAVs, GroupsX, GroupsY, GroupsZ, Sampler);
}

void FRenderer::ClearUnorderedAccess(ID3D11UnorderedAccessView* UAV, uint32 Value)
{
	if (!UAV) return;
	const uint32 Values[4] = {Value, Value, Value, Value};
	RenderCommand::GetContext()->ClearUnorderedAccessViewUint(UAV, Values);
}

void FRenderer::DispatchCompute(ID3D11ComputeShader* Shader, ID3D11Buffer* Constants,
	uint32 SRVSlot, std::span<ID3D11ShaderResourceView* const> SRVs,
	std::span<ID3D11UnorderedAccessView* const> UAVs,
	uint32 GroupsX, uint32 GroupsY, uint32 GroupsZ, ID3D11SamplerState* Sampler)
{
	if (!Shader || !GroupsX || !GroupsY || !GroupsZ || UAVs.empty()) return;
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	Context->CSSetShader(Shader, nullptr, 0);
	Context->CSSetConstantBuffers(0, 1, &Constants);
	Context->CSSetShaderResources(SRVSlot, static_cast<UINT>(SRVs.size()), SRVs.data());
	Context->CSSetUnorderedAccessViews(0, static_cast<UINT>(UAVs.size()), UAVs.data(), nullptr);
	if (Sampler) Context->CSSetSamplers(0, 1, &Sampler);
	Context->Dispatch(GroupsX, GroupsY, GroupsZ);
	
	const std::vector<ID3D11ShaderResourceView*> NullSRVs(SRVs.size(), nullptr);
	ID3D11Buffer* NullCB = nullptr;
	ID3D11SamplerState* NullSampler = nullptr;
	Context->CSSetShaderResources(SRVSlot, static_cast<UINT>(NullSRVs.size()), NullSRVs.data());
	Context->CSSetConstantBuffers(0, 1, &NullCB);
	Context->CSSetSamplers(0, 1, &NullSampler);
	Context->CSSetShader(nullptr, nullptr, 0);
}

void FRenderer::BindBatch(const FMeshDrawBatch& Batch)
{
	RenderCommand::BindPipelineState(Batch.Pipeline);
	if (Batch.bForceWireframe)
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
	
	RenderCommand::BindVertexBuffer(Batch.VertexBuffer);
	RenderCommand::BindIndexBuffer(Batch.IndexBuffer);
	
	if (Batch.bBindViewConstants)
		RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	for (uint32 Index = 0; Index < Batch.Textures.size(); ++Index)
		RenderCommand::BindShaderResource(Index, Batch.Textures[Index].Texture,
			Batch.Textures[Index].Stages);
	
	RenderCommand::BindSamplerState(0, Batch.Sampler, Batch.SamplerStages);
	
	if (Batch.MaterialDataSize)
	{
		RenderCommand::UpdateBufferData(MaterialCB.get(), Batch.MaterialData.data(), Batch.MaterialDataSize);
		RenderCommand::BindConstantBuffer(Batch.MaterialSlot, MaterialCB.get(), Batch.MaterialStages);
	}
}

void FRenderer::BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item)
{
	if (Batch.bUseObjectSlots && bUsePerObjectSlots && PerObjectSlotCB &&
		Item.ObjectSlot != InvalidObjectSlot)
	{
		RenderCommand::BindConstantBufferRange(Batch.ObjectBindingSlot, PerObjectSlotCB.get(),
			Item.ObjectSlot * PerObjectSlotConstants, PerObjectSlotConstants, Batch.ObjectStages);
	}
	else
	{
		RenderCommand::UpdateBufferData(ObjectCB.get(), Item.ObjectData.data(), Item.ObjectDataSize);
		RenderCommand::BindConstantBuffer(Batch.ObjectBindingSlot, ObjectCB.get(), Batch.ObjectStages);
	}
}

void FRenderer::DrawMeshes(std::span<const FMeshDrawBatch> Batches)
{
	uint32 PreviousTextureCount = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		if (PreviousTextureCount > Batch.Textures.size())
			RenderCommand::UnbindShaderResources(static_cast<uint32>(Batch.Textures.size()),
				PreviousTextureCount - static_cast<uint32>(Batch.Textures.size()),
				EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		
		PreviousTextureCount = static_cast<uint32>(Batch.Textures.size());
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}
}

void FRenderer::DrawFullscreen(const FPipelineState& Pipeline,
	std::span<FTexture2D* const> Textures)
{
	RenderCommand::BindPipelineState(Pipeline);
	RenderCommand::BindVertexBuffer(nullptr);
	for (uint32 Index = 0; Index < Textures.size(); ++Index)
		RenderCommand::BindShaderResource(Index, Textures[Index], EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	RenderCommand::UnbindShaderResources(0, static_cast<uint32>(Textures.size()), EShaderBindFlagBits::Pixel);
}

void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity) return;
	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount) NewCapacity *= 2;
	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer creation failed; using per-draw updates.");
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB) RenderCommand::Unmap(PerObjectSlotCB.get());
}

FTexture2D* FRenderer::GetShadowAtlas(uint32 Size)
{
	if (!ShadowAtlas || Size != ShadowAtlas->GetWidth())
	{
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Desc.Height = Size;          // 처음엔 ShadowMapMaxResolution(2048) 고정
		Desc.MipLevels = Desc.ArraySize = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		ShadowAtlas = RenderCommand::CreateTexture2D(Desc);
	}

	return ShadowAtlas.get();
}

FOcclusionMeasureResult FRenderer::MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches)
{
	FOcclusionMeasureResult Result;
	uint32 Count = 0;
	for (const FMeshDrawBatch& Batch : Batches) Count += static_cast<uint32>(Batch.Items.size());
	if (!Count) return Result;
	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	if (!Device || !Context) return Result;
	const auto StartTime = std::chrono::high_resolution_clock::now();
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf()))) return Result;
	}
	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC Desc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&Desc, Query.GetAddressOf()))) return Result;
		OcclusionQueries.Add(std::move(Query));
	}
	uint32 QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		RenderCommand::SetBlendState(EBlendState::NoColorWrite);
		Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			ID3D11Query* Query = OcclusionQueries[QueryIndex++].Get();
			Context->Begin(Query);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
			Context->End(Query);
		}
	}
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			UINT64 Samples = 0;
			while (Context->GetData(OcclusionQueries[QueryIndex++].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}
			const bool bVisible = Samples > 0;
			++Result.TotalDraws;
			Result.TotalTriangles += Item.IndexCount / 3;
			if (bVisible)
			{
				++Result.VisibleDraws;
				Result.VisibleTriangles += Item.IndexCount / 3;
			}
			if (Item.bOccludedByGpu)
			{
				++Result.GPUOccludedDraws;
				if (bVisible) ++Result.FalseCulls;
			}
			bool& bObjectVisible = ObjectVisible[Item.ObjectKey];
			bObjectVisible = bObjectVisible || bVisible;
		}
	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	Result.ElapsedMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}
