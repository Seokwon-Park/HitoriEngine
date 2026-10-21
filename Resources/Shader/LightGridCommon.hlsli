#ifndef LIGHT_GRID_COMMON_INCLUDED
#define LIGHT_GRID_COMMON_INCLUDED

// 라이트 컬링 CS가 만든 셀 목록을 PS에서 읽는 정의. 컬링 쪽은 LightCullingCommon.hlsli다.
#pragma pack_matrix(row_major)

// LightCullingCommon.hlsli의 MAX_LIGHTS_PER_CELL / CELL_STRIDE와 같아야 한다.
#define MAX_LIGHTS_PER_CELL 32
#define CELL_STRIDE (MAX_LIGHTS_PER_CELL + 1)

// ELightCullingMode와 같은 값이다.
#define LIGHT_GRID_MODE_OFF 0u
#define LIGHT_GRID_MODE_TILED 1u
#define LIGHT_GRID_MODE_CLUSTERED 2u
#define LIGHT_GRID_MODE_TILED_25D 3u

// CPU의 FForwardLightTileGrid::FLightGridConstants와 같은 64바이트 배치다.
cbuffer LightGridConstants : register(b3)
{
    uint NumTilesX;
    uint NumTilesY;
    uint NumSlices;
    uint GridMode;          // Off면 셀 목록 없이 전체 로컬 광원을 순회한다
    float GridNearZ;
    float GridFarZ;
    float SliceScale;
    float SliceBias;
    uint TileSize;
    uint bGridOrthographic;
    float GridProjZScale;   // Projection[0][2]
    float GridProjZOffset;  // Projection[3][2]
    uint GridNumLocalLights;
    uint3 LightGridPadding;
};

// 셀마다 [개수][라이트 번호 x MAX_LIGHTS_PER_CELL] 형태로 CELL_STRIDE칸씩 이어져 있다.
// 라이트 번호는 SceneLights의 로컬 광원 구간(포인트 시작 위치) 기준이다.
StructuredBuffer<uint> CellLightIndices : register(t9);

// 깊이 버퍼 값(0~1) -> View 깊이(X)
float GridDeviceZToViewDepth(float DeviceZ)
{
    return bGridOrthographic ? (DeviceZ - GridProjZOffset) / GridProjZScale // 직교: ndc.z = X·ZScale + ZOffset
                             : GridProjZOffset / (DeviceZ - GridProjZScale); // 원근: ndc.z = ZScale + ZOffset / X
}

// 픽셀이 속한 깊이 슬라이스 번호. 원근은 로그 분포, 직교는 선형
uint GetLightGridSliceIndex(float DeviceZ)
{
    if (NumSlices <= 1)
        return 0;
    const float Z = clamp(GridDeviceZToViewDepth(DeviceZ), GridNearZ, GridFarZ);
    const float Slice = bGridOrthographic ? Z * SliceScale + SliceBias : log(Z) * SliceScale + SliceBias;
    return (uint) clamp(Slice, 0.0f, NumSlices - 1.0f);
}

// 픽셀이 속한 셀(타일 x 슬라이스)의 목록이 CellLightIndices에서 시작하는 위치
uint GetLightGridCellOffset(uint2 Pixel, float DeviceZ)
{
    const uint2 Tile = Pixel / TileSize;
    const uint Slice = GetLightGridSliceIndex(DeviceZ);
    return ((Slice * NumTilesY + Tile.y) * NumTilesX + Tile.x) * CELL_STRIDE;
}

#endif
