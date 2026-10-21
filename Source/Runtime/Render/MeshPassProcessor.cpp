#include "EnginePCH.h"
#include "Render/MeshPassProcessor.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneView.h"
#include "Render/RenderResourceManager.h"
#include "Render/Mesh.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Core/EngineTimer.h"
#include <algorithm>
#include <cstring>

namespace
{
	float GetNormalEncoding(UTexture2D* Texture)
	{
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()) return 0.0f;
		switch (Texture->GetResource()->GetFormat())
		{
		case DXGI_FORMAT_BC5_UNORM: return 2.0f;
		case DXGI_FORMAT_BC5_SNORM: return 3.0f;
		default: return 1.0f;
		}
	}

	const FMatrix& PacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	uint64 GroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}

	bool BuildBatch(FMeshDrawBatch& Batch, UStaticMesh* Mesh, uint8 LOD,
		UMaterial* Material, EMeshPassType Pass, const FSceneView& View, ELightingModel LightingModel,
		const void* OverrideParams = nullptr, uint32 OverrideSize = 0)
	{
		if (!Mesh || !Material) return false;
		Batch.VertexBuffer = Mesh->GetVertexBuffer(LOD);
		Batch.IndexBuffer = Mesh->GetIndexBuffer(LOD);
		if (!Batch.VertexBuffer || !Batch.IndexBuffer) return false;
		const bool bUsesUberShader = Pass == EMeshPassType::Forward
			|| Pass == EMeshPassType::WorldNormal
			|| Pass == EMeshPassType::CSMShadowDepth;


		if (bUsesUberShader)
		{
			const bool bWorldNormal = Pass == EMeshPassType::WorldNormal;
			FPipelineState* Base = FRenderResourceManager::GetPSO(bWorldNormal
				? EPSOType::StaticMesh_WorldNormal : EPSOType::StaticMesh_Forward);
			FShaderProgram* Shader = bWorldNormal ? (Base ? Base->Shader : nullptr)
				: FRenderResourceManager::GetForwardShaderProgram(LightingModel);
			if (!Base || !Shader || !Shader->VertexShader || !Shader->PixelShader) return false;
			Batch.Pipeline = *Base;
			Batch.Pipeline.Shader = Shader;
			Batch.bUseObjectSlots = false;
			Batch.bBindViewConstants = false;
			Batch.ObjectBindingSlot = 0;
			Batch.ObjectStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			Batch.MaterialSlot = 2;
			Batch.MaterialStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			Batch.SamplerStages = Batch.MaterialStages;
			Batch.bForceWireframe = !bWorldNormal && View.bIsWireframe;

			FForwardMaterialConstants Params{};
			Params.BaseColor = Material->BaseColor;
			Params.SpecularColorShininess = FVector4(Material->SpecularColor.X,
				Material->SpecularColor.Y, Material->SpecularColor.Z, std::max(1.0f, Material->Shininess));
			UTexture2D* Color = Material->Textures.IsEmpty() ? nullptr : Material->Textures[0];
			UTexture2D* Normal = Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr;
			const bool bHasColor = Color && Color->GetResource() && Color->GetResource()->GetSRV();
			Params.SurfaceParams = FVector4(Material->bFlipNormalY ? 1.0f : 0.0f, 0.0f, bHasColor ? 1.0f : 0.0f,
				GetNormalEncoding(Normal));
			const FVector2 UV = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
			Params.UVTransform = FVector4(UV.X, UV.Y, 1.0f, 1.0f);
			std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
			Batch.MaterialDataSize = sizeof(Params);
		}
		else
		{
			EPSOType Type = Material->PSOType;
			if (Pass == EMeshPassType::GBuffer && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_GBuffer;
			else if (View.bIsWireframe && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_Wireframe;
			FPipelineState* Pipeline = FRenderResourceManager::GetPSO(Type);
			if (!Pipeline) return false;
			Batch.Pipeline = *Pipeline;
			Batch.bForceWireframe = Pass == EMeshPassType::GBuffer && View.bIsWireframe;
			if (Material->PSOType == EPSOType::StaticMesh_Opaque ||
				Material->PSOType == EPSOType::StaticMesh_Translucent ||
				Material->PSOType == EPSOType::StaticMesh_Wireframe)
			{
				FStaticMeshMaterialParams Params{};
				Params.BaseColor = Material->BaseColor;
				Params.UVOffset = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
				Params.TextureFlags.X = GetNormalEncoding(Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr);
				Params.TextureFlags.Y = Material->bFlipNormalY ? 1.0f : 0.0f;
				std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
				Batch.MaterialDataSize = sizeof(Params);
			}
			else if (OverrideParams && OverrideSize <= Batch.MaterialData.size())
			{
				std::memcpy(Batch.MaterialData.data(), OverrideParams, OverrideSize);
				Batch.MaterialDataSize = OverrideSize;
			}
		}
		Batch.Sampler = Material->SamplerState;
		Batch.Textures.reserve(Material->Textures.Num());
		for (uint32 Index = 0; Index < Material->Textures.Num(); ++Index)
		{
			// Forward 조명은 t8 이상을 전역 리소스에 사용한다.
			if (bUsesUberShader && Index >= 8) break;
			UTexture2D* Texture = Material->Textures[Index];
			EShaderBindFlagBits Stages = EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 1)
				Stages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 0 &&
				(!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()))
				Texture = nullptr;
			if (bUsesUberShader && Index == 1 && GetNormalEncoding(Texture) <= 0.5f)
				Texture = nullptr;
			Batch.Textures.push_back({ Texture, Stages });
		}
		return true;
	}

	FMeshDrawBatch::FItem BuildItem(const FMatrix& World, const FSceneView& View,
		EMeshPassType Pass, uint32 Slot, uint32 StartIndex, uint32 IndexCount,
		const void* ObjectKey, bool bOccludedByGpu, bool bReceiveShadow)
	{
		FMeshDrawBatch::FItem Item{};
		Item.ObjectSlot = Slot;
		Item.StartIndex = StartIndex;
		Item.IndexCount = IndexCount;
		Item.ObjectKey = ObjectKey;
		Item.bOccludedByGpu = bOccludedByGpu;
		if (Pass == EMeshPassType::Forward || Pass == EMeshPassType::WorldNormal
			|| Pass == EMeshPassType::CSMShadowDepth)
		{
			FForwardObjectConstants Constants{};
			Constants.World = World;
			Constants.View = View.ViewMatrix;
			Constants.Projection = View.ProjectionMatrix;
			Constants.NormalMatrix = World.Inverse().GetTransposed();
			Constants.ObjectFlags = FVector4(bReceiveShadow ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		else
		{
			const FPerObjectConstants Constants = MakePerObjectConstants(World);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		return Item;
	}
}

bool FMeshPassProcessor::Build(const FSceneView& View, const FRenderQueue& Queue,
	const FSceneRenderData* SceneData, EMeshPassType Pass,
	TArray<FMeshDrawBatch>& Out, ELightingModel LightingModel) const
{
	Out.Reset();
	if (Pass != EMeshPassType::Translucent && SceneData)
	{
		std::vector<const FStaticDrawGroup*> Groups;
		for (const FSceneRenderData::FGatherChunk& Chunk : SceneData->GatherChunks)
			for (const FStaticDrawGroup& Group : Chunk.Groups)
				if (!Group.Items.empty()) Groups.push_back(&Group);

		std::sort(Groups.begin(), Groups.end(),
			[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return GroupKey(*A) < GroupKey(*B); }); // 재질, 메시, LOD 순으로 정렬

		for (const FStaticDrawGroup* Group : Groups)
		{
			FMeshDrawBatch Batch; // Render에게 전달하는 최종 구조체
			if (!BuildBatch(Batch, Group->Mesh, Group->LODIndex, Group->Material, Pass, View, LightingModel))
				return false;
			Batch.Items.reserve(Group->Items.size());
			for (const FStaticDrawItem& Item : Group->Items)
				Batch.Items.push_back(BuildItem(Item.Proxy->GetLocalToWorld(), View, Pass, Item.Slot,
					Item.StartIndex, Item.IndexCount, Item.Proxy, Item.bOccludedByGpu != 0, Item.Proxy->bReceiveShadow));
			Out.Add(std::move(Batch));
		}
	}
	const uint32 Begin = Pass == EMeshPassType::Translucent ? Queue.GetFirstTranslucentIndex() : 0;
	const uint32 End = Pass == EMeshPassType::Translucent ? Queue.Num() : Queue.GetFirstTranslucentIndex();
	Out.Reserve(Out.size() + End - Begin);
	for (uint32 Index = Begin; Index < End; ++Index)
	{
		const FRenderPacket& Packet = Queue[Index];
		if (Pass == EMeshPassType::WorldNormal &&
			(!Packet.Material || Packet.Material->PSOType != EPSOType::StaticMesh_Opaque)) continue;
		FMeshDrawBatch Batch;
		if (!BuildBatch(Batch, Packet.Mesh, Packet.LODIndex, Packet.Material, Pass, View,
			LightingModel, Packet.MaterialParamData, Packet.MaterialParamDataSize)) return false;

		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Batch.IndexBuffer->GetIndexCount();
		const void* ObjectKey = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);

		Batch.Items.push_back(BuildItem(PacketWorld(Packet), View, Pass, Packet.Slot,
			Packet.StartIndex, IndexCount, ObjectKey, Packet.bOccludedByGpu, Packet.Proxy ? Packet.Proxy->bReceiveShadow : true));
		Out.Add(std::move(Batch));
	}
	return true;
}
