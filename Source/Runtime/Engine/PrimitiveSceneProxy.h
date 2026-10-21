#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;

class FPrimitiveSceneProxy
{
public:
    explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent) : Component(InComponent) {}
    ~FPrimitiveSceneProxy();
    void UpdateTransform();
    void UpdateRenderState();
    void OnMeshRenderDataChanged();
    void OnMeshDestroyed();

    UPrimitiveComponent* GetComponent() const { return Component; }
    const FAABB& GetBounds() const { return Bounds; }
    const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
    const FMatrix& GetWorldToLocal() const { return WorldToLocal; }
    const FLODSphere& GetLODSphere() const { return LODSphere; }
    UStaticMesh* GetMesh() const { return Mesh; }
    bool IsVisible() const { return bVisible; }
    bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
    uint8 GetLODCount() const { return RenderState ? RenderState->LODCount : 1; }
    const FMeshRenderState* GetRenderState() const { return RenderState.get(); }
    const float* GetLODThresholdsSq() const { return RenderState->LODThresholdSq; }
    const FCachedMeshLOD& GetLOD(uint32 Index) const { return RenderState->LODs[Index]; }
    const FCachedMeshSection& GetSection(uint32 Index) const { return RenderState->Sections[Index]; }
    FScene* GetScene() const { return Scene; }

    bool bCastDynamicShadow = true;
    bool bReceiveShadow = true;
private:
    friend class FScene;
    FScene* Scene = nullptr;
    UPrimitiveComponent* Component = nullptr;
    UStaticMesh* Mesh = nullptr;
    TSharedPtr<const FMeshRenderState> RenderState;
    FMatrix LocalToWorld;
    FMatrix WorldToLocal;
    FAABB Bounds;
    FLODSphere LODSphere;
    int32 PackedIndex = INDEX_NONE;
    bool bQueuedForUpdate = false;
    bool bRenderStateQueued = false;
    bool bVisible = true;
    bool bHiddenInGame = false;
};
