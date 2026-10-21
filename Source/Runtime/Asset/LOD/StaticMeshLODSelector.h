#pragma once
#include "Core/Types.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"

using FLODViewContext = FViewContext;

template<bool Orthographic>
inline uint32 SelectSphereLOD(const FLODSelectionInput& Input, const FLODViewContext& View)
{
    const FMeshRenderState* State = Input.State;
    if (!State || State->LODCount <= 1 || View.Width == 0 || View.Height == 0) return 0;
    const FLODSphere& Sphere = Input.Sphere;
    const float Depth = Sphere.Center.X * View.CameraForward.X
        + Sphere.Center.Y * View.CameraForward.Y + Sphere.Center.Z * View.CameraForward.Z - View.CameraDepth;
    const float NearDistance = Depth - View.NearZ;
    if (NearDistance <= 0.0f || NearDistance * NearDistance <= Sphere.RadiusSquared) return 0;
    const float Numerator = Sphere.RadiusSquared * View.ProjectionScaleSquared;
    const float DistanceFactor = Orthographic ? 1.0f : Depth * Depth;
    uint32 DesiredLOD = 3;
    if (Numerator >= State->LODThresholdSq[0] * DistanceFactor) DesiredLOD = 0;
    else if (Numerator >= State->LODThresholdSq[1] * DistanceFactor) DesiredLOD = 1;
    else if (Numerator >= State->LODThresholdSq[2] * DistanceFactor) DesiredLOD = 2;
    return DesiredLOD < State->LODCount ? DesiredLOD : State->LODCount - 1;
}

inline uint32 SelectLOD(const FPrimitiveSceneProxy& Proxy, const FLODViewContext& View)
{
    const FLODSelectionInput Input{Proxy.GetLODSphere(), Proxy.GetRenderState()};
    return View.bOrthographic ? SelectSphereLOD<true>(Input, View) : SelectSphereLOD<false>(Input, View);
}

// Recomputed for every current view; previous selections are never reused.
void SelectLODs(const TArray<FLODSelectionInput>& Inputs, const FLODViewContext& View, TArray<uint8>& OutLODs);
