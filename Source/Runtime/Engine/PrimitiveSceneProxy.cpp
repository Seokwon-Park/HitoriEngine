#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Render/Mesh.h"
#include "Engine/Scene.h"

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
	if (Mesh) Mesh->UnregisterProxy(this);
}

void FPrimitiveSceneProxy::OnMeshRenderDataChanged()
{
	if (Scene)
	{
		Scene->MarkRenderStateDirty(this);
		Scene->MarkDirty(this);
	}
}

void FPrimitiveSceneProxy::OnMeshDestroyed()
{
	Mesh = nullptr;
	RenderState.reset();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	LocalToWorld = Component->GetWorldMatrix();
	WorldToLocal = LocalToWorld.Inverse();
	const FBox LocalBounds = Mesh ? Mesh->GetRenderBounds() : Component->CalcLocalBounds();
	Bounds = MakeWorldBounds(LocalBounds.GetWorldAABB(LocalToWorld));
	LODSphere = Mesh ? Mesh->GetLocalLODSphere().Transform(LocalToWorld) : FLODSphere{};
}

void FPrimitiveSceneProxy::UpdateRenderState()
{
	UStaticMesh* PreviousMesh = Mesh;
	Mesh = nullptr;
	RenderState.reset();
	bVisible = Component->IsShown(false);
	bCastDynamicShadow = Component->CastShadow && Component->bCastDynamicShadow;
	bReceiveShadow = Component->bReceiveShadow;
	bHiddenInGame = !Component->IsShown(true);
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	if (StaticMeshComponent) Mesh = StaticMeshComponent->GetStaticMesh();
	if (Mesh != PreviousMesh)
	{
		if (PreviousMesh) PreviousMesh->UnregisterProxy(this);
		if (Mesh) Mesh->RegisterProxy(this);
		if (Scene) Scene->MarkDirty(this);
	}
	if (!Mesh) return;
	std::vector<UMaterial*> Materials;
	Materials.reserve(Mesh->MeshData.MaterialSlots.Num());
	for (uint32 Slot = 0; Slot < static_cast<uint32>(Mesh->MeshData.MaterialSlots.Num()); ++Slot)
		Materials.push_back(StaticMeshComponent->GetMaterial(Slot));
	RenderState = Mesh->GetRenderState(Materials);
}
