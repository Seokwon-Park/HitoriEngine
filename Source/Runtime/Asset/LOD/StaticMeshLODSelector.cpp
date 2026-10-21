#include "EnginePCH.h"
#include "StaticMeshLODSelector.h"
#include "Render/Mesh.h"
#include <immintrin.h>
#include <cstddef>

static_assert(offsetof(FLODSphere, RadiusSquared) == 12 && sizeof(FLODSphere) == 16);

namespace
{
    template<bool Orthographic>
    void SelectBatch(const TArray<FLODSelectionInput>& Inputs, const FLODViewContext& View, TArray<uint8>& OutLODs)
    {
        const __m128 FX = _mm_set1_ps(View.CameraForward.X);
        const __m128 FY = _mm_set1_ps(View.CameraForward.Y);
        const __m128 FZ = _mm_set1_ps(View.CameraForward.Z);
        const __m128 CameraDepth = _mm_set1_ps(View.CameraDepth);
        const __m128 NearZ = _mm_set1_ps(View.NearZ);
        const __m128 ProjectionScale = _mm_set1_ps(View.ProjectionScaleSquared);
        uint32 I = 0;
        for (; I + 4 <= static_cast<uint32>(Inputs.Num()); I += 4)
        {
            bool bScalar = false;
            for (uint32 Lane = 0; Lane < 4; ++Lane)
                bScalar |= !Inputs[I + Lane].State || Inputs[I + Lane].State->LODCount <= 1;
            if (bScalar)
            {
                for (uint32 Lane = 0; Lane < 4; ++Lane)
                    OutLODs[I + Lane] = static_cast<uint8>(SelectSphereLOD<Orthographic>(Inputs[I + Lane], View));
                continue;
            }
            __m128 X = _mm_loadu_ps(&Inputs[I].Sphere.Center.X);
            __m128 Y = _mm_loadu_ps(&Inputs[I + 1].Sphere.Center.X);
            __m128 Z = _mm_loadu_ps(&Inputs[I + 2].Sphere.Center.X);
            __m128 R2 = _mm_loadu_ps(&Inputs[I + 3].Sphere.Center.X);
            _MM_TRANSPOSE4_PS(X, Y, Z, R2);
            const __m128 Depth = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(X, FX),
                _mm_mul_ps(Y, FY)), _mm_mul_ps(Z, FZ)), CameraDepth);
            const __m128 NearDistance = _mm_sub_ps(Depth, NearZ);
            const int NearMask = _mm_movemask_ps(_mm_or_ps(_mm_cmple_ps(NearDistance, _mm_setzero_ps()),
                _mm_cmple_ps(_mm_mul_ps(NearDistance, NearDistance), R2)));
            const __m128 Numerator = _mm_mul_ps(R2, ProjectionScale);
            const __m128 Factor = Orthographic ? _mm_set1_ps(1.0f) : _mm_mul_ps(Depth, Depth);
            int Masks[3];
            for (uint32 T = 0; T < 3; ++T)
            {
                const __m128 Thresholds = _mm_set_ps(Inputs[I + 3].State->LODThresholdSq[T], Inputs[I + 2].State->LODThresholdSq[T],
                    Inputs[I + 1].State->LODThresholdSq[T], Inputs[I].State->LODThresholdSq[T]);
                Masks[T] = _mm_movemask_ps(_mm_cmpge_ps(Numerator, _mm_mul_ps(Thresholds, Factor)));
            }
            for (uint32 Lane = 0; Lane < 4; ++Lane)
            {
                const int Bit = 1 << Lane;
                const uint32 Count = Inputs[I + Lane].State->LODCount;
                const uint32 LOD = ((NearMask | Masks[0]) & Bit) ? 0 : (Masks[1] & Bit) ? 1 : (Masks[2] & Bit) ? 2 : 3;
                OutLODs[I + Lane] = static_cast<uint8>(Count <= 1 ? 0 : LOD < Count ? LOD : Count - 1);
            }
        }
        for (; I < static_cast<uint32>(Inputs.Num()); ++I)
            OutLODs[I] = static_cast<uint8>(SelectSphereLOD<Orthographic>(Inputs[I], View));
    }
}

void SelectLODs(const TArray<FLODSelectionInput>& Inputs, const FLODViewContext& View, TArray<uint8>& OutLODs)
{
    OutLODs.SetNum(Inputs.Num(), false);
    if (View.Width == 0 || View.Height == 0)
    {
        for (uint8& LOD : OutLODs) LOD = 0;
        return;
    }
    if (View.bOrthographic) SelectBatch<true>(Inputs, View, OutLODs);
    else SelectBatch<false>(Inputs, View, OutLODs);
}
