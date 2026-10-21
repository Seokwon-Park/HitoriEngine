// Clustered(타일 × 깊이 슬라이스)와 Tiled(슬라이스 1개) 라이트 컬링.
// 그룹 하나 = 셀 하나. 그룹 안 스레드들이 라이트를 나눠서 검사한다.
#include "LightCullingCommon.hlsli"

#define THREADS_PER_CELL 64

// 슬라이스 경계의 View 깊이. 원근은 로그 분포, 직교는 선형
float SliceBoundary(uint Slice)
{
    const float t = (float) Slice / NumSlices;
    return bOrthographic ? lerp(NearZ, FarZ, t) : NearZ * pow(abs(FarZ / NearZ), t);
}

[numthreads(THREADS_PER_CELL, 1, 1)]
void mainCS(uint3 GroupID : SV_GroupID, uint GroupIndex : SV_GroupIndex)
{
    // 그룹 내에서 공유되는 공용 카운터는 한 스레드만 초기화
    // 스레드 간 순서가 보장되지 않지만 Barrier를 통해 0으로 초기화가 끝날때까지 대기한다.
    if (GroupIndex == 0)
        sLightCount = 0;
    GroupMemoryBarrierWithGroupSync();

    const FCullCell Cell = MakeCullCell(GroupID.xy, SliceBoundary(GroupID.z), SliceBoundary(GroupID.z + 1));

    // 스레드 64개가 라이트를 나눠 검사한다. 0번 스레드는 0, 64, 128... 번 라이트를 맡는다.
    for (uint i = GroupIndex; i < NumLocalLights; i += THREADS_PER_CELL)
    {
        float3 CenterVS;
        float CullRadius;
        if (LightIntersectsCell(LocalLights[LocalLightOffset + i], Cell, CenterVS, CullRadius))
            AppendLight(i);
    }
    GroupMemoryBarrierWithGroupSync();

    // 셀 번호 = (슬라이스, 행, 열) 순서. PS의 GetTileBase와 같은 식이어야 한다
    const uint CellIndex = (GroupID.z * NumTilesY + GroupID.y) * NumTilesX + GroupID.x;
    WriteCellList(CellIndex, GroupIndex, THREADS_PER_CELL);
}
