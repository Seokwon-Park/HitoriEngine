#include "EnginePCH.h"
#include "StaticMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Asset/LOD/StaticMeshLODSelector.h"


// StaticMesh 컴포넌트를 초기화한다.
UStaticMeshComponent::UStaticMeshComponent()
{
	StaticMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
}

// StaticMesh 컴포넌트의 소멸을 처리한다.
UStaticMeshComponent::~UStaticMeshComponent()
{
}

// 부모 컴포넌트의 시작 처리를 호출한다.
void UStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UStaticMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (StaticMesh == InStaticMesh)
		return;

	StaticMesh = InStaticMesh;
	ClearOverrideMaterials();
	MarkRenderStateDirty();
	OnTransformDirty();
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	return StaticMesh ? static_cast<int32>(StaticMesh->GetMeshData().MaterialSlots.Num()) : 0;
}

FString UStaticMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	if (!StaticMesh || SlotIndex < 0 || SlotIndex >= GetNumMaterials())
		return FString();
	return StaticMesh->GetMeshData().MaterialSlots[SlotIndex].Name;
}

UMaterial* UStaticMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	return StaticMesh ? StaticMesh->GetMaterial(static_cast<uint32>(SlotIndex)) : nullptr;
}

void UStaticMeshComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
	if (!StaticMesh) return;
	uint32 SelectedLOD = 0;
	if (const FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		SelectedLOD = SelectLOD(*Proxy, ViewContext);
	}
	SubmitToRenderQueue(RenderQueue, SelectedLOD);
}

// 지정한 LOD의 Section으로 패킷을 만든다.
void UStaticMeshComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, uint32 LODIndex)
{
    const FPrimitiveSceneProxy* Proxy = GetSceneProxy();
    if (!StaticMesh || !Proxy) return;
    const FMeshRenderState* State = Proxy->GetRenderState();
    if (!State) return;
    if (LODIndex >= State->LODCount) LODIndex = 0;
    const FCachedMeshLOD& LOD = State->LODs[LODIndex];
    for (uint32 I = 0; I < LOD.NumSections; ++I)
    {
        const FCachedMeshSection& Section = State->Sections[LOD.FirstSection + I];
        FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
        Packet.Proxy = Proxy;
        Packet.Mesh = StaticMesh;
        Packet.Material = Section.Material;
        Packet.StartIndex = Section.StartIndex;
        Packet.IndexCount = Section.IndexCount;
        Packet.LODIndex = static_cast<uint8>(LODIndex);
    }
}
/*
// Section별 Material·Texture와 인덱스 범위를 보존해 패킷을 제출한다.
void UStaticMeshComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	if (!StaticMesh)
		return;

	const FStaticMeshData& MeshData = StaticMesh->GetMeshData();

	for (const FStaticMeshSection& Section : MeshData.Sections)
	{
		// 슬롯마다 덮어쓰기가 있으면 그것, 없으면 메시(OBJ/MTL)의 기본 머티리얼
		UMaterial* SectionMaterial = GetMaterial(static_cast<int32>(Section.MaterialSlotIndex));
		if (!SectionMaterial)
			continue;

		FRenderPacket rp;
		rp.mesh = StaticMesh;
		rp.model = GetWorldMatrix();
		rp.StartIndex = Section.StartIndex;
		rp.IndexCount = Section.IndexCount;
		rp.material = SectionMaterial;

		RenderQueue.Enqueue(rp);
	}
}
*/
