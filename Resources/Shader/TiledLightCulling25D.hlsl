// 2.5D Tiled 라이트 컬링 (Harada 2012).
// 그룹 하나 = 16×16 타일, 스레드 하나 = 픽셀 하나.
// Depth Prepass 결과로 타일의 실제 깊이 범위 [MinZ, MaxZ]를 재고, 그 범위를 32칸으로 나눈 깊이 마스크로
// "픽셀이 실제로 있는 깊이"와 겹치는 라이트만 남긴다. 출력 형식은 Clustered와 같고 슬라이스는 1개다.
#include "LightCullingCommon.hlsli"

#define TILE_DIM 16                             // C++ TiledTileSize와 같아야 한다
#define THREADS_PER_TILE (TILE_DIM * TILE_DIM)

Texture2D<float> DepthTexture : register(t11);  // R24_UNORM_X8 -> 0~1 깊이

groupshared uint sMinZ;         // View 깊이(양수 float)의 비트. 양수 float는 비트 대소 = 값 대소
groupshared uint sMaxZ;
groupshared uint sDepthMask;    // [MinZ, MaxZ]를 32칸으로 나눠 픽셀이 있는 칸에 1

// 깊이 버퍼 값(0~1) -> View 깊이(X)
float ToViewDepth(float D)
{
    const float ZScale = Projection[0][2];
    const float ZOffset = Projection[3][2];
    return bOrthographic ? (D - ZOffset) / ZScale       // 직교: ndc.z = X·ZScale + ZOffset
                         : ZOffset / (D - ZScale);      // 원근: ndc.z = ZScale + ZOffset / X
}

[numthreads(TILE_DIM, TILE_DIM, 1)]
void mainCS(uint3 GroupID : SV_GroupID, uint3 DispatchID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    if (GroupIndex == 0)
    {
        sMinZ = 0x7F7FFFFF;     // FLT_MAX
        sMaxZ = 0;
        sDepthMask = 0;
        sLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 1) 자기 픽셀 깊이. 화면 밖과 아무것도 안 그려진 곳(깊이 1)은 제외
    const bool bInside = all(DispatchID.xy < (uint2) ViewportSize);
    // viewport 안쪽이면 픽셀의 Depth 값을 가져온다.
    const float Depth = bInside ? DepthTexture.Load(int3(DispatchID.xy, 0)) : 1.0f;
    // Depth가 1.0 미만이면 유효한 깊이
    const bool bValid = Depth < 1.0f;
    // 
    const float Z = bValid ? ToViewDepth(Depth) : 0.0f;

    // 2) 타일 min/max
    if (bValid)
    {
        InterlockedMin(sMinZ, asuint(Z));
        InterlockedMax(sMaxZ, asuint(Z));
    }
    GroupMemoryBarrierWithGroupSync();

    const float MinZ = asfloat(sMinZ);
    const float MaxZ = asfloat(sMaxZ);
    const bool bEmpty = MinZ > MaxZ;                    // 픽셀이 하나도 없는 타일 (그룹 전체가 같은 값)
    const float InvBinSize = 32.0f / max(MaxZ - MinZ, 1e-4f);

    // 3) 깊이 마스크
    if (bValid)
    {
        const uint Bin = min((uint) ((Z - MinZ) * InvBinSize), 31u);
        InterlockedOr(sDepthMask, 1u << Bin);
    }
    GroupMemoryBarrierWithGroupSync();

    // 4) 라이트 검사: 타일 측면 + [MinZ, MaxZ] AABB (+ Spot 원뿔) -> 깊이 마스크
    if (!bEmpty)
    {
        const FCullCell Cell = MakeCullCell(GroupID.xy, MinZ, MaxZ);
        const uint DepthMask = sDepthMask;

        for (uint i = GroupIndex; i < NumLocalLights; i += THREADS_PER_TILE)
        {
            float3 CenterVS;
            float CullRadius;
            if (!LightIntersectsCell(LocalLights[LocalLightOffset + i], Cell, CenterVS, CullRadius))
                continue;

            // 라이트 구가 덮는 깊이 칸들. 범위 겹침은 위에서 확인했으므로 Lo <= Hi
            const int Lo = clamp((int) floor((CenterVS.x - CullRadius - MinZ) * InvBinSize), 0, 31);
            const int Hi = clamp((int) floor((CenterVS.x + CullRadius - MinZ) * InvBinSize), 0, 31);
            const uint LightMask = (0xFFFFFFFFu >> (31 - (Hi - Lo))) << Lo;

            // 라이트가 닿는 깊이에 실제 픽셀이 하나도 없으면 제외 (타일 안의 빈 공간)
            if ((LightMask & DepthMask) != 0)
                AppendLight(i);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 5) 슬라이스가 1개이므로 셀 번호 = 타일 번호
    const uint CellIndex = GroupID.y * NumTilesX + GroupID.x;
    WriteCellList(CellIndex, GroupIndex, THREADS_PER_TILE);
}
