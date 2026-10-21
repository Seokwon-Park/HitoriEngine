#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"
// Todo: subuv
cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// BillboardComponent.h의 FSubUVConstants와 바이트 단위로 같아야 한다.
cbuffer subuv : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
    float4 Tint;
    // 1: HDR 출력일 때 sRGB -> 선형 변환, 0: 현재 LDR 반투명 패스에서는 그대로 사용
    float DecodeSRGB;
    float3 SubUVPadding;
};

Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Todo: VS shader code duplicated
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.uv = input.uv;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float atlasCol = fmod(CurrentFrame, AtlasColSize);
    float atlasRow = floor(CurrentFrame / AtlasColSize);

    float2 cellSize = float2(1.0f / AtlasColSize, 1.0f / AtlasRowSize);
    float2 cellUV = input.uv * cellSize;

    float2 cellStartOffsetUV = (float2(atlasCol, atlasRow) * cellSize);
    cellUV += cellStartOffsetUV;

    float4 color = AtlasTexture.Sample(AtlasSampler, cellUV);
    color.a *= ParticleAlpha;
    if (DecodeSRGB > 0.5f)
    {
        // 텍스처와 Tint를 각각 선형으로 바꾼 뒤 곱한다. 알파는 색이 아니므로 그대로.
        color.rgb = SRGBToLinear(color.rgb) * SRGBToLinear(Tint.rgb);
    }
    else
    {
        color.rgb *= Tint.rgb;
    }

    return color;
}
