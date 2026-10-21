#pragma once

#include "Containers/Array.h"
#include "Math/Box.h"
#include "Math/Sphere.h"

class UMaterial;

struct FCachedMeshSection
{
    UMaterial* Material;
    uint32 StartIndex;
    uint32 IndexCount;
};

struct FCachedMeshLOD
{
    uint32 FirstSection = 0;
    uint32 NumSections = 0;
};

// Shared by objects with the same mesh and resolved material slots.
// These are asset properties, never cached visibility or LOD decisions.
struct FMeshRenderState
{
    TArray<FCachedMeshSection> Sections;
    FCachedMeshLOD LODs[4];
    float LODThresholdSq[3]{};
    uint8 LODCount = 1;
};

struct FLODSelectionInput
{
    FLODSphere Sphere;
    const FMeshRenderState* State = nullptr;
};
