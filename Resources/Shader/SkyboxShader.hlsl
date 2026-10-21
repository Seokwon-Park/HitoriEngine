#pragma pack_matrix(row_major)
// 등장방형(equirectangular) 파노라마를 배경으로 그린다.
// 정점 버퍼 없이 화면을 덮는 삼각형 하나를 SV_VertexID로 만든다.

#include "ColorSpace.hlsli"

cbuffer SkyboxConstants : register(b0)
{
    matrix InverseViewProjection;
    float3 CameraPosition;
    // 1이면 셰이더에서 sRGB -> 선형 변환. (8bit/BC UNORM 텍스처)
    // _SRGB 포맷은 하드웨어가 샘플링할 때 이미 디코딩하고, float(.hdr) 포맷은 원래 선형이라 0이다.
    float  DecodeSRGB;
};

Texture2D    PanoramaTexture : register(t0);
SamplerState LinearSampler   : register(s0);

static const float PI = 3.14159265f;

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;

    // VertexID 0,1,2 -> UV (0,0) (2,0) (0,2)
    // 화면보다 큰 삼각형 하나로 전체를 덮는다. 사각형 두 장보다 대각선 이음매가 없다.
    Output.UV = float2((VertexID << 1) & 2, VertexID & 2);

    // UV [0,2] -> NDC [-1,3]. y는 위아래가 뒤집혀 있다.
    Output.Position = float4(Output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);

    return Output;
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    // 화면 좌표 -> 원평면 위의 월드 좌표 -> 카메라가 그쪽을 보는 방향
    float2 NDC = Input.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    float4 FarPoint = mul(float4(NDC, 1.0f, 1.0f), InverseViewProjection);
    FarPoint /= FarPoint.w;                       // 원근 나눗셈. 빼먹으면 화면 가장자리가 틀어진다

    float3 Direction = normalize(FarPoint.xyz - CameraPosition);

    // 방향 -> 등장방형 UV
    // 엔진 좌표계는 Z-up / X-forward / Y-right (언리얼 규약).
    // 따라서 수평면은 XY, 위쪽은 Z다. Y-up 기준 공식을 쓰면 하늘이 누워서 나온다.
    float2 PanoUV;
    PanoUV.x = atan2(Direction.y, Direction.x) / (2.0f * PI) + 0.5f;  // 경도: XY 평면
    PanoUV.y = acos(clamp(Direction.z, -1.0f, 1.0f)) / PI;            // 위도: +Z가 천정

    // Sample이 아니라 SampleLevel.
    // atan2가 -PI/+PI 경계에서 U를 1.0 -> 0.0으로 점프시키는데,
    // Sample은 그 UV 미분을 보고 최하위 밉을 골라 세로로 흐릿한 줄을 만든다.
    float4 Color = PanoramaTexture.SampleLevel(LinearSampler, PanoUV, 0);
    if (DecodeSRGB > 0.5f)
    {
        Color.rgb = SRGBToLinear(Color.rgb);
    }
    return float4(Color.rgb, 1.0f);
}
