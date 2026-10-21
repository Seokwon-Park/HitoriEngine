#include "ColorSpace.hlsli"

// ToneMapRenderer.h의 FToneMapConstants와 바이트 단위로 같아야 한다.
cbuffer ToneMapConstants : register(b0)
{
    float Exposure;     // 기본 1.0 (FToneMapRenderer::SetExposure로 조절)
    uint  Mode;         // 0 = ACES + sRGB 인코딩(Lit), 1 = sRGB 인코딩만(Unlit), 2 = 그대로 출력(WorldNormal·Heatmap 등 디버그)
    float2 ToneMapPadding;
};

#define TONEMAP_MODE_ACES        0
#define TONEMAP_MODE_ENCODE_ONLY 1
#define TONEMAP_MODE_PASSTHROUGH 2

Texture2D InputTexture : register(t0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

// Krzysztof Narkowicz의 ACES Filmic 근사. 입력과 출력 모두 선형 값이다.
float3 ToneMapACES(float3 X)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return saturate((X * (A * X + B)) / (X * (C * X + D) + E));
}

PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2); // (0,0) (2,0) (0,2)
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    Output.UV = UV;
    return Output;
}

float4 mainPS(PSInput Input) : SV_Target
{
    const float3 SceneColor = max(InputTexture.Load(int3(Input.Position.xy, 0)).rgb, 0.0f);

    // 디버그 뷰는 셰이더가 쓴 값을 그대로 보여준다.
    if (Mode == TONEMAP_MODE_PASSTHROUGH)
    {
        return float4(saturate(SceneColor), 1.0f);
    }

    // Unlit은 텍스처 원본 색을 보여야 하므로 곡선 없이 인코딩만 한다.
    const float3 Mapped = (Mode == TONEMAP_MODE_ENCODE_ONLY)
        ? saturate(SceneColor)
        : ToneMapACES(SceneColor * Exposure);

    // 씬은 선형으로 쌓였으므로 화면에 내보내기 전에 한 번만 sRGB로 인코딩한다.
    // ImGui가 뷰포트 텍스처를 알파 블렌딩으로 그리므로 알파는 1로 고정한다.
    return float4(LinearToSRGB(Mapped), 1.0f);
}
