#pragma pack_matrix(row_major)
cbuffer GridCB : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPos;
    int CellSize;
    float SubCellSize;
    int GridPlaneType;
    float2 Padding;
};

static const float GridSize = 400.0f; 
static const float4 Positions[4] =
{
    float4(-0.5, 0.5, 0.0, 1.0),
    float4(0.5, 0.5, 0.0, 1.0),
    float4(-0.5, -0.5, 0.0, 1.0),
    float4(0.5, -0.5, 0.0, 1.0)
};

struct PS_INPUT
{
	// 원근 Grid Pixel Shader에 화면 위치·월드 평면 좌표·카메라 위치를 전달한다.
    float4 pos : SV_POSITION;
    float2 coords : TEXCOORD0;
    float3 camPos : TEXCOORD1;
};

// 카메라 XY를 중심으로 큰 XY 평면 Quad를 배치해 원근 Grid의 Raster 영역을 만든다.
PS_INPUT mainVS(uint vid : SV_VertexID)
{
    PS_INPUT o;
    float4 localPos = Positions[vid];
    float4 worldPos = float4(0, 0, 0, 1);

    worldPos.xyz = localPos.xyz * GridSize;
    worldPos.xy += CameraPos.xy;
    o.coords = worldPos.xy;

    o.pos = mul(worldPos, ViewProj);
    o.camPos = CameraPos;
    return o;
}

#define MOD(x,y) ((x) - (y) * floor((x)/(y))) 

static const float4 CellColor = float4(1.0, 1.0, 1.0, 0.6);
static const float4 SubCellColor = float4(1.0, 1.0, 1.0, 0.35);

static const float HeightToFadeRatio = 25.0f;
static const float MinFadeDistance = 5.0f;
static const float MaxFadeDistance = 50.0f;

// 흰 Grid에 거리 페이드를 적용하며 월드 축은 별도 공통 픽셀 두께 경로에서 그린다.
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float fadeDist = clamp(abs(input.camPos.z) * HeightToFadeRatio, MinFadeDistance, MaxFadeDistance);

    // 화면 1픽셀당 월드 좌표의 변화량
    float2 derivative = max(fwidth(input.coords), float2(1.0e-6, 1.0e-6));

    // 값 / (해당 픽셀의 변화량)을 통해 화면 픽셀(Screen Space) 단위의 거리를 구합니다.
    float2 subGrid = abs(frac(input.coords / SubCellSize - 0.5) - 0.5) * SubCellSize / derivative;
    float subLineDist = min(subGrid.x, subGrid.y);
    float alphaSub = 1.0 - min(subLineDist, 1.0); // 두께 1픽셀
    
    float2 mainGrid = abs(frac(input.coords / CellSize - 0.5) - 0.5) * CellSize / derivative;
    float mainLineDist = min(mainGrid.x, mainGrid.y);
    float alphaCell = 1.0 - min(mainLineDist / 1.5, 1.0); // 두께 1.5픽셀


    float4 color = float4(SubCellColor.rgb, 0.0);
    
    color = lerp(color, SubCellColor, alphaSub);
    color = lerp(color, CellColor, alphaCell);

    float distToCamera = length(input.coords - input.camPos.xy);
    float falloff = smoothstep(1.0, 0.5, distToCamera / fadeDist);

    color.a *= falloff;

    if (color.a < 0.01)
        discard;
        
    return color;
}
