#include "EnginePCH.h"
#include "Mesh.h"
#include "Renderer.h"
#include "Asset/AssetManager.h"
#include "Engine/PrimitiveSceneProxy.h"
#include <algorithm>
#include <cmath>
#include <limits>

UStaticMesh::~UStaticMesh()
{
    for (FPrimitiveSceneProxy* Proxy : RenderProxies)
        Proxy->OnMeshDestroyed();
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
}

UMaterial* UStaticMesh::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < Materials.size() && Materials[SlotIndex]) {
		return Materials[SlotIndex];
	}
	return UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
}

size_t UStaticMesh::FMaterialSlotsHash::operator()(const std::vector<UMaterial*>& Slots) const
{
    size_t Hash = 0;
    for (const UMaterial* Material : Slots)
        Hash ^= std::hash<const UMaterial*>{}(Material) + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
    return Hash;
}

void UStaticMesh::RebuildRenderData()
{
    RenderBounds = MeshData.AABB;
    for (const auto& LOD : AdditionalLODs)
    {
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            RenderBounds.Min[Axis] = std::min(RenderBounds.Min[Axis], LOD.Data.AABB.Min[Axis]);
            RenderBounds.Max[Axis] = std::max(RenderBounds.Max[Axis], LOD.Data.AABB.Max[Axis]);
        }
    }
    LocalLODSphere.Center = (RenderBounds.Min + RenderBounds.Max) * 0.5f;
    double RadiusSquared = 0.0;
    for (uint32 LOD = 0; LOD < GetLODCount(); ++LOD)
    {
        for (const FVertex& Vertex : GetMeshData(LOD).Vertices)
        {
            const double X = static_cast<double>(Vertex.Position.X) - LocalLODSphere.Center.X;
            const double Y = static_cast<double>(Vertex.Position.Y) - LocalLODSphere.Center.Y;
            const double Z = static_cast<double>(Vertex.Position.Z) - LocalLODSphere.Center.Z;
            RadiusSquared = std::max(RadiusSquared, X * X + Y * Y + Z * Z);
        }
    }
    // Include a small geometric margin and round outwards.
    LocalLODSphere.RadiusSquared = std::nextafter(
        static_cast<float>(RadiusSquared * (1.0 + 2e-5)), std::numeric_limits<float>::infinity());
    RenderStates.clear();
    for (FPrimitiveSceneProxy* Proxy : RenderProxies)
        Proxy->OnMeshRenderDataChanged();
}

TSharedPtr<const FMeshRenderState> UStaticMesh::GetRenderState(const std::vector<UMaterial*>& Materials)
{
    auto& WeakState = RenderStates[Materials];
    if (auto State = WeakState.lock()) return State;
    auto State = MakeShared<FMeshRenderState>();
    State->LODCount = static_cast<uint8>(std::min(GetLODCount(), 4u));
    for (uint32 I = 0; I < 3; ++I)
        State->LODThresholdSq[I] = ScreenThresholds[I] * ScreenThresholds[I];
    for (uint32 LOD = 0; LOD < State->LODCount; ++LOD)
    {
        auto& Range = State->LODs[LOD];
        Range.FirstSection = State->Sections.Num();
        for (const FStaticMeshSection& Section : GetMeshData(LOD).Sections)
        {
            UMaterial* Material = Section.MaterialSlotIndex < Materials.size()
                ? Materials[Section.MaterialSlotIndex] : GetMaterial(Section.MaterialSlotIndex);
            if (Material) State->Sections.Add({ Material, Section.StartIndex, Section.IndexCount });
        }
        Range.NumSections = State->Sections.Num() - Range.FirstSection;
    }
    WeakState = State;
    return State;
}

void UStaticMesh::RegisterProxy(FPrimitiveSceneProxy* Proxy)
{
    RenderProxies.Add(Proxy);
}

void UStaticMesh::UnregisterProxy(FPrimitiveSceneProxy* Proxy)
{
    const auto It = std::find(RenderProxies.begin(), RenderProxies.end(), Proxy);
    if (It != RenderProxies.end())
        RenderProxies.RemoveAtSwap(static_cast<uint32>(It - RenderProxies.begin()));
}
