cbuffer FXAAConstants : register(b0)
{
    // 한 픽셀의 UV 크기
    float InvWidth;
    float InvHeight;
    float2 Padding;
};

// 입력은 color buffer 한 장뿐. 깊이·법선은 읽지 않는다.
Texture2D<float4> OriginalColorBufferTexture : register(t0);
// bilinear 샘플러. 쌍 평균과 최종 섞기를 하드웨어 보간에 맡기는 전제
SamplerState LinearClampSampler : register(s0);


static const float EdgeThreshold = 0.166; // 상대 임계값 (rangeMax에 비례)
static const float EdgeThresholdMin = 0.0833; // 절대 임계값 (어두운 곳 제외)
static const float SubpixelStrength = 0.75;
// UE Quality 4 = FXAA preset 29. 마지막 간격은 미발견 방향의 거리 확장에만 사용한다.
static const int SearchSampleCount = 11;
static const float SearchSteps[12] = { 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

// luma = 0.299R + 0.587G + 0.114B (Rec.601). UE의 FxaaLuma와 같음
float GetLuma(float3 Color)
{
    return dot(Color, float3(0.299, 0.587, 0.114));
}

// bilinear로 읽은 색의 luma.
float SampleLuma(float2 UV)
{
    return GetLuma(OriginalColorBufferTexture.SampleLevel(LinearClampSampler, UV, 0).rgb);
}

struct VSOutput
{
    float4 Position : SV_Position;
};

// 풀스크린 패스 (큰 삼각형)
VSOutput mainVS(uint VertexID : SV_VertexID)
{
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    VSOutput Output;
    Output.Position = float4(UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return Output;
}


float4 mainPS(VSOutput Input) : SV_Target
{
    float2 TexelSize = float2(InvWidth, InvHeight);
    float2 UV = Input.Position.xy * TexelSize;
    float4 OriginalColor = OriginalColorBufferTexture.SampleLevel(LinearClampSampler, UV, 0);
    
    // M과 상하좌우 4개의 luma
    float M = GetLuma(OriginalColor.rgb);
    float N = SampleLuma(UV + float2(0, -TexelSize.y));
    float S = SampleLuma(UV + float2(0, TexelSize.y));
    float W = SampleLuma(UV + float2(-TexelSize.x, 0));
    float E = SampleLuma(UV + float2(TexelSize.x, 0));

    // range = 주변 5픽셀의 최대 − 최소, 기준은 상대값과 절대 하한 중 큰 값
    float MinLuma = min(M, min(min(N, S), min(W, E)));
    float MaxLuma = max(M, max(max(N, S), max(W, E)));
    float Range = MaxLuma - MinLuma;
    if (Range < max(EdgeThresholdMin, MaxLuma * EdgeThreshold))
        return OriginalColor;

    // 모서리 4개는 조기 종료 뒤에야 읽는다 
    float NW = SampleLuma(UV + float2(-TexelSize.x, -TexelSize.y));
    float NE = SampleLuma(UV + float2(TexelSize.x, -TexelSize.y));
    float SW = SampleLuma(UV + float2(-TexelSize.x, TexelSize.y));
    float SE = SampleLuma(UV + float2(TexelSize.x, TexelSize.y));
    
    // 3×3 이차 차분, 가운데 줄 ×2 : 양옆 줄 ×1
    float EdgeH = 2 * abs(N + S - 2 * M) + abs(NW + SW - 2 * W) + abs(NE + SE - 2 * E);
    float EdgeV = 2 * abs(W + E - 2 * M) + abs(NW + NE - 2 * N) + abs(SW + SE - 2 * S);
    bool Horizontal = EdgeH >= EdgeV;

    // 가로·세로를 한 코드로 처리하려고 선을 가로지르는 두 이웃을 Negative/Positive로 재정의
    float NegativeLuma = Horizontal ? N : W;
    float PositiveLuma = Horizontal ? S : E;
    // 반대편 이웃 고르기. 기울기가 큰 쪽, 동점이면 위/왼쪽 (pairN)
    bool UseNegative = abs(NegativeLuma - M) >= abs(PositiveLuma - M);
    float NeighborLuma = UseNegative ? NegativeLuma : PositiveLuma;
    // 반대편 이웃 쪽으로 한 픽셀 (선에 수직)
    float2 BlendStep = Horizontal ? float2(0, TexelSize.y) : float2(TexelSize.x, 0);
    BlendStep *= UseNegative ? -1.0 : 1.0;
    // 선을 따라가는 한 픽셀 (탐색 방향)
    float2 SearchStep = Horizontal ? float2(TexelSize.x, 0) : float2(0, TexelSize.y);

    // 경계를 가로질러 반 픽셀 이동하면 bilinear 샘플이 쌍 평균이 된다.
    float PairAverage = (M + NeighborLuma) * 0.5;
    // 탐색 끝 판정 기준 = contrast0 / 4 (gradientScaled)
    float EndThreshold = abs(NeighborLuma - M) * 0.25;
    // M과 반대편 이웃 사이 경계선 위. 여기서 bilinear로 읽으면 쌍 평균이 한 번에 나온다.
    float2 SearchOrigin = UV + BlendStep * 0.5;
    bool FoundNegative = false;
    bool FoundPositive = false;
    float DistanceNegative = 0;
    float DistancePositive = 0;
    float EndDeltaNegative = 0;
    float EndDeltaPositive = 0;

    // 샘플 거리: 1, 2.5, 4.5, ..., 18.5, 22.5픽셀. 발견한 방향은 멈춘다.
    [loop]
    for (int Step = 0; Step < SearchSampleCount; ++Step)
    {
        if (!FoundNegative)
        {
            DistanceNegative += SearchSteps[Step];
            EndDeltaNegative = SampleLuma(SearchOrigin - SearchStep * DistanceNegative) - PairAverage;
            FoundNegative = abs(EndDeltaNegative) >= EndThreshold;
        }
        if (!FoundPositive)
        {
            DistancePositive += SearchSteps[Step];
            EndDeltaPositive = SampleLuma(SearchOrigin + SearchStep * DistancePositive) - PairAverage;
            FoundPositive = abs(EndDeltaPositive) >= EndThreshold;
        }
        if (FoundNegative && FoundPositive)
            break;
    }

    // UE처럼 미발견 방향은 30.5픽셀까지 거리만 확장한다.
    // 부호 검사에는 마지막으로 실제 샘플링한 밝기 차이를 사용한다.
    if (!FoundNegative)
        DistanceNegative += SearchSteps[SearchSampleCount];
    if (!FoundPositive)
        DistancePositive += SearchSteps[SearchSampleCount];

    float EdgeOffset = 0;
    float NearestEndDelta = DistanceNegative < DistancePositive ? EndDeltaNegative : EndDeltaPositive;
    if ((NearestEndDelta < 0) != (M - PairAverage < 0))
        // 양쪽 미발견이면 거리가 같아 자연스럽게 0이 된다.
        EdgeOffset = 0.5 - min(DistanceNegative, DistancePositive) / (DistanceNegative + DistancePositive);

    // 서브픽셀: 주변 8픽셀 가중평균 (상하좌우 2, 모서리 1, 합 12). M 제외
    float NeighborhoodAverage = (2 * (N + S + W + E) + NW + NE + SW + SE) / 12.0;
    float ContrastRatio = saturate(abs(NeighborhoodAverage - M) / max(Range, 1e-6));
    float SmoothContrast = ContrastRatio * ContrastRatio * (3 - 2 * ContrastRatio);
    float SubpixelOffset = SmoothContrast * SmoothContrast * SubpixelStrength;
    
    // t나 subpixel 값 중에서 더 큰 값을 바탕으로 섞는다.
    float2 FinalUV = UV + BlendStep * max(EdgeOffset, SubpixelOffset);
    
    // bilinear 보간으로 RGB와 알파를 함께 섞는다.
    return OriginalColorBufferTexture.SampleLevel(LinearClampSampler, FinalUV, 0);
}
