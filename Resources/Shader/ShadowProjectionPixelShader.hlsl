#pragma pack_matrix(row_major)

// 화면 픽셀마다 Shadow Map과 비교해 Mask(1 = 밝음, 0 = 그림자)를 만든다.
// 언리얼 ShadowProjectionPixelShader.usf에 해당.
Texture2D<float> SceneDepth : register(t0);
Texture2D<float> ShadowDepth : register(t1);
SamplerComparisonState ShadowSampler : register(s0);

cbuffer ShadowProjection : register(b0)
{
    matrix InverseViewProjection;
    matrix ShadowViewProjection;
    float4 ShadowUVScaleOffset; // xy: 배율, zw: 오프셋
    float4 ShadowParams; // x: 텍셀 크기, y: 비교 bias
    float4 ViewportSize;
    float4 CascadeParams; // x: SplitNear, y: SplitFar
    float4 CameraPosition;
    float4 CameraForward;
    float4 PCSSParams;
};

struct PSInput
{
    float4 Position : SV_POSITION;
};

// 16점 Poisson disk (단위 원 안에 고르게 퍼진 샘플)
static const float2 Poisson16[16] =
{
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

// 칸 UV(0~1) -> atlas UV. 칸 밖으로 나가지 않게 반 텍셀 안쪽으로 clamp
float2 TileToAtlas(float2 TileUV, float HalfTexel)
{
    TileUV = clamp(TileUV, HalfTexel, 1.0f - HalfTexel);
    return TileUV * ShadowUVScaleOffset.xy + ShadowUVScaleOffset.zw;
}

float DirectionalPCSS(float2 TileUV, float Receiver, float2 SvPos)
{
    const float MapSize = 1.0f / ShadowParams.x;
    const float TileTexel = ShadowParams.x / ShadowUVScaleOffset.x; // 칸 UV 기준 텍셀 1개
    const float HalfTexel = 0.5f * TileTexel;
    const float MaxKernel = PCSSParams.y;

    // 픽셀마다 샘플 패턴을 돌려 줄무늬(banding)를 노이즈로 바꾼다 (언리얼은 Sobol 회전)
    const float Angle = 6.2831853f * frac(52.9829189f * frac(dot(SvPos, float2(0.06711056f, 0.00583715f))));
    const float2x2 Rot = float2x2(cos(Angle), -sin(Angle), sin(Angle), cos(Angle));

    // Blocker 탐색: 수신점보다 빛에 가까운 깊이들의 평균
    const float SearchRadius = clamp(Receiver * PCSSParams.x, TileTexel, MaxKernel);
    float BlockerSum = 0.0f;
    float BlockerCount = 0.0f;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        const float2 UV = TileToAtlas(TileUV + mul(Poisson16[i], Rot) * SearchRadius, HalfTexel);
        const float D = ShadowDepth.Load(int3(UV * MapSize, 0)); // 비교 말고 깊이 값 자체
        if (D < Receiver)
        {
            BlockerSum += D;
            BlockerCount += 1.0f;
        }
    }
    if (BlockerCount == 0.0f)
        return 1.0f; // 가리는 게 없음 -> 완전히 밝음

    
    // 반그림자 반경 = (수신 깊이 - blocker 평균 깊이) × tan × 깊이범위 / 폭
    const float Blocker = BlockerSum / BlockerCount;
    const float FilterRadius = clamp((Receiver - Blocker) * PCSSParams.x, TileTexel, MaxKernel);
    
    // 그 반경으로 PCF
    float Sum = 0.0f;
    [unroll]
    for (int j = 0; j < 16; ++j)
    {
        const float2 UV = TileToAtlas(TileUV + mul(Poisson16[j], Rot) * FilterRadius, HalfTexel);
        Sum += ShadowDepth.SampleCmpLevelZero(ShadowSampler, UV, Receiver);
    }
    return Sum / 16.0f;
}

//// 칸 UV 1당 깊이가 얼마나 변하나 (수신 평면의 기울기)
//float2 ReceiverPlaneDepthBias(float3 Dx, float3 Dy)
//{
//    float2 BiasUV;
//    BiasUV.x = Dy.y * Dx.z - Dx.y * Dy.z;
//    BiasUV.y = Dx.x * Dy.z - Dy.x * Dx.z;
//    const float Det = Dx.x * Dy.y - Dx.y * Dy.x;
//    return BiasUV / (abs(Det) > 1e-12f ? Det : 1e-12f);
//}

// ToneMapShader와 같은 화면 삼각형
PSInput mainVS(uint VertexID : SV_VertexID)
{
    PSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(PSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float Depth = SceneDepth.Load(int3(Pixel, 0));
    if (Depth >= 1.0f)
        return 1.0f; // 하늘

    // DeferredLightingShader와 같은 위치 복원
    float2 UV = (float2(Pixel) + 0.5f) / ViewportSize.xy;
    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    if (abs(WorldH.w) < 1.0e-6f)
        return 1.0f;
    float3 World = WorldH.xyz / WorldH.w;
    
    const float ViewDepth = dot(World - CameraPosition.xyz, CameraForward.xyz);
    if (ViewDepth < CascadeParams.x || ViewDepth >= CascadeParams.y)
        return 1.0f;

    // 그림자 카메라 공간으로
    float4 S = mul(float4(World, 1.0f), ShadowViewProjection);
    if (S.w <= 0.0f)
        return 1.0f; // 라이트 뒤쪽
    S.xyz /= S.w;
    float2 ShadowUV = float2(S.x * 0.5f + 0.5f, 0.5f - S.y * 0.5f);
    if (any(ShadowUV < 0.0f) || any(ShadowUV > 1.0f) || S.z > 1.0f)
        return 1.0f;
    
    // 53~54줄 범위 검사 뒤, 55줄 atlas 변환 전
    const float Receiver = S.z - ShadowParams.y;
    if (PCSSParams.x > 0.0f)
        return DirectionalPCSS(ShadowUV, Receiver, Input.Position.xy).xxxx;
    
    ShadowUV = ShadowUV * ShadowUVScaleOffset.xy + ShadowUVScaleOffset.zw;

    // 3x3 PCF
    float Sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
        for (int x = -1; x <= 1; ++x)
            Sum += ShadowDepth.SampleCmpLevelZero(ShadowSampler,
               ShadowUV + float2(x, y) * ShadowParams.x, S.z - ShadowParams.y);

    return (Sum / 9.0f).xxxx;  // R 채널 = 채널 0
}