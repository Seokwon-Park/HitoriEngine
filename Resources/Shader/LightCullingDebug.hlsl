// UberLit과 동일한 상수 배치 및 셀 주소 계산을 사용한다.
#include "LightGridCommon.hlsli"

// 타일 크기는 컬링 방식마다 달라서 상수 버퍼의 TileSize를 쓴다. Off는 TileSize가 0이라 16칸으로 표시한다.
#define TILE_SIZE max(TileSize, 16u)

Texture2D<float> SceneDepth : register(t0);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    // 정점 버퍼 없이 기존 ToneMap과 같은 전체 화면 삼각형을 생성한다.
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float3 GetTileColor(uint Count)
{
    const uint ColorPeriod = 16;
    float Hue = float((Count - 1u) % ColorPeriod) / float(ColorPeriod);

    // Hue를 RGB로 변환하여 한 주기 안에서도 여러 색상을 사용한다.
    float3 RGB = saturate(
        abs(frac(Hue + float3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f) - 1.0f);
    return RGB * 0.85f + 0.10f;
}

// 0~9의 3×4 픽셀 글꼴이다. 비트 위치는 행×3+열이다.
static const uint DigitMasks[10] =
{
    3951, 3738, 3751, 3895, 2541, 3983, 3961, 1191, 3967, 2543
};

bool IsCountPixel(uint Count, uint2 InTile)
{
    // 0도 표시하며 uint의 최대 10자리까지 숫자를 그대로 출력한다.
    uint DigitCount = 1;
    for (uint Remaining = Count; Remaining >= 10; Remaining /= 10)
        ++DigitCount;

	// 줄 바꿈 계산
	uint RowCount = (DigitCount + 3) / 4;
	uint FirstRowDigits = DigitCount - (RowCount - 1) * 4;
	uint TextHeight = RowCount * 5 - 1;

	// 타일 크기 기준값
	const uint ReferenceTileSize = 16u;

	// 타일 배율 계산
	const uint DigitScale = max(1u, TILE_SIZE / ReferenceTileSize);

	// 화면 좌표를 글꼴 좌표로 환산
	TextHeight *= DigitScale;
	int Y = int(InTile.y) - int((TILE_SIZE - TextHeight) / 2);
	if (Y < 0 || Y >= int(TextHeight))
		return false;
    Y = int(uint(Y) / DigitScale);
	if (uint(Y) % 5 == 4)
		return false;

	// 너비와 중앙 정렬 적용
	uint Row = uint(Y) / 5;
	uint RowDigits = Row == 0 ? FirstRowDigits : 4;
	uint TextWidth = (RowDigits * 4 - 1) * DigitScale;
	int X = int(InTile.x) - int((TILE_SIZE - TextWidth + 1) / 2);
	if (X < 0 || X >= int(TextWidth))
		return false;
    X = int(uint(X) / DigitScale);
	if (uint(X) % 4 == 3)
		return false;

    // 현재 글자의 자릿수만 추출한다.
    uint BeforeRow = Row == 0 ? 0 : FirstRowDigits + (Row - 1) * 4;
    uint DigitIndex = BeforeRow + uint(X) / 4;
    for (uint Shift = DigitCount - DigitIndex - 1; Shift > 0; --Shift)
        Count /= 10;

    uint Bit = (uint(Y) % 5) * 3 + uint(X) % 4;
    return ((DigitMasks[Count % 10] >> Bit) & 1) != 0;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    uint2 Pixel = uint2(Input.Position.xy);
    float DeviceZ = SceneDepth.Load(int3(Pixel, 0));

    // 목록이 없거나 불투명 표면이 없는 픽셀은 기존 장면을 유지한다.
    if (DeviceZ >= 1.0f) discard;

    uint2 ViewSize;
    SceneDepth.GetDimensions(ViewSize.x, ViewSize.y);

    // 후보 광원이 있는 타일의 표면에만 표시한다.
    uint2 Tile = Pixel / TILE_SIZE;
    uint2 InTile = Pixel % TILE_SIZE;

    // 숫자는 타일 중앙 표면의 대표 셀에서 가져온 하나의 값으로 고정한다.
    uint2 CenterPixel = min(Tile * TILE_SIZE + TILE_SIZE / 2u, ViewSize - 1u);
    float CenterDeviceZ = SceneDepth.Load(int3(CenterPixel, 0));
    if (CenterDeviceZ < 1.0f)
    {
        uint TextCount = GridNumLocalLights;
        if (GridMode != LIGHT_GRID_MODE_OFF)
            TextCount = CellLightIndices[GetLightGridCellOffset(CenterPixel, CenterDeviceZ)];

        // 현재 표면의 후보가 0이어도 대표 숫자의 획은 먼저 출력한다.
        if (TextCount > 0u && IsCountPixel(TextCount, InTile)) return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Off는 전체 입력 수, 나머지는 현재 표면 셀의 후보 수를 사용한다.
    uint Count = GridNumLocalLights;
    if (GridMode != LIGHT_GRID_MODE_OFF)
        Count = CellLightIndices[GetLightGridCellOffset(Pixel, DeviceZ)];
    if (Count == 0)
        discard;

    // 기존 나머지 연산 기반 색상 주기를 그대로 사용한다.
    float3 Color = GetTileColor(Count);
    float Alpha = 0.40f;
    if (InTile.x == 0 || InTile.y == 0)
    {
        Color = float3(0.08f, 0.08f, 0.08f);
        Alpha = 0.70f;
    }

    // 상한을 넘어 전체 순회로 전환된 셀을 표시한다.
    bool CellOverflow = GridMode != LIGHT_GRID_MODE_OFF && Count >= MAX_LIGHTS_PER_CELL;
    if (CellOverflow && InTile.x >= TILE_SIZE - 3u && InTile.y >= 1u && InTile.y <= 2u)
    {
        Color = float3(1.0f, 0.0f, 1.0f);
        Alpha = 1.0f;
    }
    return float4(Color, Alpha);
}
