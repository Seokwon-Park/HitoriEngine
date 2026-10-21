#pragma pack_matrix(row_major)
cbuffer TransformBuffer : register(b0)
{
    matrix World;
    matrix ViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PSInput mainVS(VSInput Input)
{
    PSInput Output;
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(WorldPos, ViewProjection);
    Output.UV = Input.UV;
    return Output;
}

Texture2D MSDFTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer MSDFParams : register(b1)
{
    float PxRange; // msdf-atlas-gen에서 지정한 distance range (예: 4.0)
    //float ScreenPxRange; // 화면상 실제 표현 픽셀 범위 (동적 계산 또는 고정값)
    float3 Padding;
};

float Median(float3 msdf)
{
    return max(min(msdf.r, msdf.g), min(max(msdf.r, msdf.g), msdf.b));
}

// 실제 화면 픽셀 기준 distance range를 UV 미분값으로 동적 계산
float ScreenPxRangeDynamic(float2 UV, float2 TextureSize)
{
    float2 UnitRange = float2(PxRange, PxRange) / TextureSize;
    float2 ScreenTexSize = float2(1.0, 1.0) / fwidth(UV);
    return max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    float3 Sample = MSDFTexture.Sample(LinearSampler, Input.UV).rgb;
    float SignedDist = Median(Sample) - 0.5f;

	// 텍스처 크기를 알고 있다면 동적 계산 (권장)
    float TexWidth, TexHeight;
    MSDFTexture.GetDimensions(TexWidth, TexHeight);
    float PxRangeScreen = ScreenPxRangeDynamic(Input.UV, float2(TexWidth, TexHeight));

    float ScreenPxDistance = PxRangeScreen * SignedDist;
    float Opacity = saturate(ScreenPxDistance + 0.5f);

	// 가장자리가 완전히 0인 픽셀은 discard (배경 겹침 방지, 알파 블렌딩 안 쓸 경우 유용)
    clip(Opacity - 0.001f);

    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}