// 라이트 컬링 CS(Clustered/Tiled, 2.5D)가 함께 쓰는 정의와 검사 함수.
// 구조체·cbuffer는 C++(SceneManagement.h, ForwardLightTiles.h)과 바이트 단위로 같아야 한다.
// 셀 목록에는 LocalLightOffset을 뺀 로컬 광원 번호를 저장한다.
#pragma pack_matrix(row_major)
#include "LightData.hlsli"

#define MAX_LIGHTS_PER_CELL 32
#define CELL_STRIDE (MAX_LIGHTS_PER_CELL + 1)

cbuffer FLightCullingConstants : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float2 ViewportSize;
    uint NumTilesX;
    uint NumTilesY;
    uint NumLocalLights;
    uint NumSlices;
    uint TileSize;
    uint bOrthographic;
    float NearZ;
    float FarZ;
    uint LocalLightOffset;  // LocalLights에서 로컬 광원(포인트, 스포트, 렉트) 구간이 시작되는 위치
    uint CullingPadding;
};

StructuredBuffer<FLightRenderParameters> LocalLights : register(t10);
RWStructuredBuffer<uint> TileLightIndices : register(u0);

groupshared uint sLightCount;
groupshared uint sLightIndices[MAX_LIGHTS_PER_CELL];

// 컬링용 구. Spot은 원뿔을 감싸는 최소 구, Rect는 사각형 대각선 반만큼 넓힌 구.
void GetCullSphere(FLightRenderParameters L, out float3 Center, out float Radius)
{
    Center = L.Position;
    Radius = L.Radius;

    if (L.Type == LIGHT_TYPE_RECT)
    {
        // 사각형 위 가장 가까운 점부터 Radius만큼 닿으므로 대각선 반만큼 늘린다
        Radius += length(float2(L.HalfWidth, L.HalfHeight));
    }
    else if (L.Type == LIGHT_TYPE_SPOT)
    {
        const float CosA = L.OuterConeCos;
        const float SinA = sqrt(saturate(1.0f - CosA * CosA));

        if (CosA < 0.70710678f)     // 바깥 각도 > 45도: 넓은 원뿔
        {
            Center = L.Position + L.Direction * (L.Radius * CosA);
            Radius = L.Radius * SinA;
        }
        else                        // 바깥 각도 <= 45도: 좁은 원뿔
        {
            Center = L.Position + L.Direction * (L.Radius / (2.0f * CosA));
            Radius = L.Radius / (2.0f * CosA);
        }
    }
}

// 타일의 화면 범위를 NDC로. T가 위(+), B가 아래(-)
void GetTileNDC(uint2 Tile, out float L, out float R, out float T, out float B)
{
    const float2 PixMin = Tile * TileSize;
    const float2 PixMax = PixMin + TileSize;
    L = 2.0f * PixMin.x / ViewportSize.x - 1.0f;
    R = 2.0f * PixMax.x / ViewportSize.x - 1.0f;
    T = 1.0f - 2.0f * PixMin.y / ViewportSize.y;
    B = 1.0f - 2.0f * PixMax.y / ViewportSize.y;
}

float4 NormalizePlane(float4 P)
{
    return P / length(P.xyz);
}

// 타일 측면 4개 평면 (View 공간). 깊이 방향은 셀마다 Near/Far로 따로 검사한다.
void BuildTileSidePlanes(uint2 Tile, out float4 Planes[4])
{
    // Projection 행렬의 열 성분을 index로 한번에 가져오기 위해서
    const float4x4 ProjTrans = transpose(Projection);

    float L, R, T, B;
    GetTileNDC(Tile, L, R, T, B);

    Planes[0] = NormalizePlane(ProjTrans[0] - L * ProjTrans[3]);
    Planes[1] = NormalizePlane(R * ProjTrans[3] - ProjTrans[0]);
    Planes[2] = NormalizePlane(ProjTrans[1] - B * ProjTrans[3]);
    Planes[3] = NormalizePlane(T * ProjTrans[3] - ProjTrans[1]);
}

// 타일 × [Near, Far] 조각을 감싸는 View 공간 AABB
void BuildCellAABB(uint2 Tile, float Near, float Far, out float3 Min, out float3 Max)
{
    float L, R, T, B;
    GetTileNDC(Tile, L, R, T, B);

    if (bOrthographic)
    {
        // clip.x = Y * XScale + XOffset  ->  Y = (u - XOffset) / XScale
        const float YMin = (L - Projection[3][0]) / Projection[1][0];
        const float YMax = (R - Projection[3][0]) / Projection[1][0];
        const float ZMin = (B - Projection[3][1]) / Projection[2][1];
        const float ZMax = (T - Projection[3][1]) / Projection[2][1];

        Min = float3(Near, YMin, ZMin);
        Max = float3(Far, YMax, ZMax);
    }
    else
    {
        // 원근: Y = u * d / XScale. 코너 8개 중 축마다 최소·최대를 고른다
        const float4 Ys = float4(L * Near, L * Far, R * Near, R * Far) / Projection[1][0];
        const float4 Zs = float4(B * Near, B * Far, T * Near, T * Far) / Projection[2][1];

        Min = float3(Near, min(min(Ys.x, Ys.y), min(Ys.z, Ys.w)), min(min(Zs.x, Zs.y), min(Zs.z, Zs.w)));
        Max = float3(Far,  max(max(Ys.x, Ys.y), max(Ys.z, Ys.w)), max(max(Zs.x, Zs.y), max(Zs.z, Zs.w)));
    }
}

bool SphereInTile(float3 CenterVS, float Radius, float4 Planes[4], float Near, float Far)
{
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (dot(Planes[i].xyz, CenterVS) + Planes[i].w < -Radius)
            return false;
    }
    return CenterVS.x + Radius >= Near && CenterVS.x - Radius <= Far;
}

bool SphereIntersectsAABB(float3 C, float R, float3 Min, float3 Max)
{
    const float3 D = max(0, max(Min - C, C - Max));
    return dot(D, D) <= R * R;
}

// Bart Wronski - Cull that Cone
bool SphereOutsideCone(float3 ConePos, float3 ConeDir, float Range,
                       float CosA, float SinA, float3 S, float SR)
{
    const float3 V = S - ConePos;
    const float VLen = dot(V, ConeDir);
    const float Perp = sqrt(max(dot(V, V) - VLen * VLen, 0.0f));
    const float Dist = CosA * Perp - SinA * VLen;

    const bool bOutsideSide = Dist > SR;
    const bool bBeyondRange = VLen > Range + SR;
    const bool bBehindApex = VLen < -SR;
    return bOutsideSide || bBeyondRange || bBehindApex;
}

// 셀 정보를 한 번에 들고 다니기 위한 묶음
struct FCullCell
{
    float4 Planes[4];
    float Near;
    float Far;
    float3 Min;
    float3 Max;
    float3 Center;      // AABB를 감싸는 구 (원뿔 검사용)
    float Radius;
};

FCullCell MakeCullCell(uint2 Tile, float Near, float Far)
{
    FCullCell Cell;
    BuildTileSidePlanes(Tile, Cell.Planes);
    Cell.Near = Near;
    Cell.Far = Far;
    BuildCellAABB(Tile, Near, Far, Cell.Min, Cell.Max);
    Cell.Center = (Cell.Min + Cell.Max) * 0.5f;
    Cell.Radius = length(Cell.Max - Cell.Min) * 0.5f;
    return Cell;
}

// 라이트가 셀과 겹치는지. 통과하면 View 공간 컬링 구를 돌려준다 (2.5D 깊이 마스크에 쓴다).
bool LightIntersectsCell(FLightRenderParameters L, FCullCell Cell, out float3 CenterVS, out float CullRadius)
{
    float3 Center;
    GetCullSphere(L, Center, CullRadius);
    CenterVS = mul(float4(Center, 1.0f), View).xyz;

    if (!SphereInTile(CenterVS, CullRadius, Cell.Planes, Cell.Near, Cell.Far) ||
        !SphereIntersectsAABB(CenterVS, CullRadius, Cell.Min, Cell.Max))
        return false;

    if (L.Type == LIGHT_TYPE_SPOT)
    {
        const float3 ConePos = mul(float4(L.Position, 1.0f), View).xyz;
        const float3 ConeDir = normalize(mul(float4(L.Direction, 0.0f), View).xyz);
        const float SinA = sqrt(saturate(1.0f - L.OuterConeCos * L.OuterConeCos));
        if (SphereOutsideCone(ConePos, ConeDir, L.Radius, L.OuterConeCos, SinA, Cell.Center, Cell.Radius))
            return false;
    }
    return true;
}

// 통과한 라이트를 그룹 공용 목록에 추가. 상한을 넘으면 개수만 늘고 번호는 버린다
void AppendLight(uint LightIndex)
{
    uint Slot;
    InterlockedAdd(sLightCount, 1, Slot);   // Slot = 더하기 전 값 = 내가 쓸 칸
    if (Slot < MAX_LIGHTS_PER_CELL)
        sLightIndices[Slot] = LightIndex;
}

// 그룹 공용 목록을 전역 버퍼의 셀 구역 [개수][번호들]로 복사. 모든 스레드가 호출한다
void WriteCellList(uint CellIndex, uint GroupIndex, uint NumThreads)
{
    const uint Base = CellIndex * CELL_STRIDE;
    const uint Count = min(sLightCount, MAX_LIGHTS_PER_CELL);

    if (GroupIndex == 0)
        TileLightIndices[Base] = Count;

    for (uint j = GroupIndex; j < Count; j += NumThreads)
        TileLightIndices[Base + 1 + j] = sLightIndices[j];
}
