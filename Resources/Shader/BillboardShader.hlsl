#pragma pack_matrix(row_major)

cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// UBillboardComponent가 제출하는 FSubUVConstants와 동일한 레이아웃이다.
cbuffer MaterialParams : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float SpriteAlpha;
    float4 Tint;
    float DecodeSRGB;
    float3 Padding;
};

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

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float4 WorldPosition = mul(float4(input.position, 1.0f), World);
    output.position = mul(WorldPosition, VP);
    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = SpriteTexture.Sample(SpriteSampler, input.uv);
    color.rgb *= Tint.rgb;
    color.a *= SpriteAlpha;
    return color;
}
