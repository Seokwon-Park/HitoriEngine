#pragma pack_matrix(row_major)

cbuffer HeightFogConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4 FogColor;
    float4 DensityHeight; // density, falloff, reference height, start distance
    float4 DistanceViewport; // max opacity, cutoff distance, inverse width/height
};
Texture2D<float> SceneDepthTexture : register(t0);

struct VSOutput { float4 Position : SV_Position; };

VSOutput mainVS(uint VertexID : SV_VertexID)
{
    // Scene Depth와 동일하게 Full-Screen Quad로 그린다.
    static const float2 Positions[6] = {
        float2(-1, 1), float2(1, 1), float2(-1, -1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };
    VSOutput Output;
    Output.Position = float4(Positions[VertexID], 0, 1);
    return Output;
}

//   깊이로 점의 위치 복원 -> 시작/차단 거리 처리 -> 선적분 τ
//   -> 투과율 T = e^-τ -> 최대 불투명도 하한-> 블렌딩으로 색 합성
// 출력은 (안개 색, 알파 = 1 - T)이고, 블렌딩이 "새 색 = 안개 색 * (1 - T) + 기존 색 * T"를 만든다.
float4 mainPS(VSOutput Input) : SV_Target
{
    // [입력] 이 픽셀의 깊이 버퍼 값(DeviceZ). 정방향 깊이라 NDC z와 같은 값이다.
    // Scene Depth 구현과 같은 방식으로 Load한다(SRV로 읽기만 하고 깊이 테스트/쓰기는 끈 패스).
    float Depth = SceneDepthTexture.Load(int3(int2(Input.Position.xy), 0));

    // 비정상 값(NaN/Inf, 음수)은 안개를 적용하지 않는다.
    if (!isfinite(Depth) || Depth < 0.0f)
        return 0;

    // 픽셀 좌표(px, py)를 화면 크기로 나눠 0~1로 만든다. 아래에서 -1~1(정규화 좌표 nx, ny)로 바꾼다.
    float2 UV = Input.Position.xy * DistanceViewport.zw;

    // (nx, ny) = (UV.x*2-1, 1-UV.y*2)이고 y는 화면 좌표가 아래로 증가하므로 뒤집는다.
    // z는 깊이 버퍼 값이다. 이 NDC 점에 역 ViewProjection을 곱하고 동차 좌표 w로 나누면 월드 위치가 나온다.
    float4 World = mul(float4(UV.x * 2 - 1, 1 - UV.y * 2, Depth, 1), InverseViewProjection);
    float3 Position = World.xyz / World.w;

    // 카메라에서 점까지의 벡터(월드 축 기준). 이 벡터의 z가 높이 차 Δh이다.
    float3 Ray = Position - CameraPosition.xyz;
    // 카메라에서 점까지의 직선거리 d. 앞쪽 깊이 z_v가 아니라 벡터의 길이를 쓴다.
    float Distance = length(Ray);
    // 시작 거리 s0. 안개를 세기 시작하는 거리(카메라 앞쪽은 맑게).
    float Start = DensityHeight.w;

    // 점이 시작 거리 안쪽이면 세는 구간이 비어 τ = 0, T = 1이다(안개 없음).
    if (Distance <= Start)
        return 0;
    // 차단 거리(0이면 꺼짐)를 넘으면 안개를 아예 끈다(T = 1). 경계에서 T가 갑자기 뛸 수 있다.
    if (DistanceViewport.y > 0 && Distance > DistanceViewport.y)
        return 0;

    // 세는 구간의 길이 L = d - s0 (시작 거리 이후 구간만 적분한다).
    float Length = Distance - Start;
    // 시작점(카메라에서 거리 s0인 광선 위의 점)의 높이 hs = 카메라 높이 + Δh * (s0 / d).
    // 광선이 직선이라 높이가 거리에 선형으로 변한다.
    float StartHeight = CameraPosition.z + Ray.z * (Start / Distance);
    // 높이 감쇠율 k (1/월드 길이). 밀도는 높이에 따라 e^(-k(h - h0))로 줄어든다.
    float Falloff = DensityHeight.y;
    // a = |k * (he - hs)|: 구간의 시작 높이와 끝 높이 차에 감쇠율을 곱한 값(u의 절댓값).
    float A = abs(Falloff * (Position.z - StartHeight));
    // 구간에서 더 낮은 쪽 끝의 밀도 지수 -k * (hLow - h0). h0 = 기준 높이(DensityHeight.z).
    float HeightExponent = -Falloff * (min(StartHeight, Position.z) - DensityHeight.z);

    // 낮은 끝점에서 시작한 적분으로 바꿔 양의 큰 지수를 피한다.
    // 보정계수 g(a) = (1 - e^-a) / a. 
    // a가 0에 가까우면 0/0이므로 극한(1)을 테일러 근사로 쓴다(1 - a/2 + a^2/6).
    float G;
    if (A < 0.01f) 
        G = 1 - A * 0.5f + A * A / 6;
    else 
        G = (1 - exp(-A)) / A;
    // τ = density * L * e^(-k(hLow-h0)) * g 의 자연로그.
    //   logτ = ln(density) + ln(L) + (-k(hLow-h0)) + ln(g)
    // 곱 대신 로그의 합으로 계산해 큰 지수의 곱이 float를 넘는 것을 막는다. (density <= 0이면 log가 -inf가 되어
    // 아래 isfinite에서 걸러지고 안개가 적용되지 않는다.)
    float LogTau = log(DensityHeight.x) + log(Length) + HeightExponent + log(G);
    if (!isfinite(LogTau)) 
        return 0;
    // 투과율 T = e^(-τ), τ = e^(logτ). τ가 80 이상이면 T를 0으로, 매우 작으면 1로 생략한다(오차는 e^-80 이하).
    float T;
    if (LogTau >= 4.382026635f) T = 0; // log(80): 생략 오차 exp(-80) 이하
    else if (LogTau <= -80) T = 1;
    else T = exp(-exp(LogTau));
    // 최대 불투명도 M(DistanceViewport.x)의 하한: T가 (1 - M) 아래로 내려가지 않게 한다.
    T = max(saturate(T), 1 - DistanceViewport.x);
    // SRC_ALPHA / INV_SRC_ALPHA가 원래 장면 색에 T를 곱한다.
    // [4단계] 새 색 = FogColor * (1 - T) + 기존 장면 색 * T, 즉 최종 색 = c * T + f * (1 - T)이다.
    // (감쇠는 기존 색에 곱하고, 안개가 새로 내는 빛은 더하는 구조)
    return float4(FogColor.rgb, 1 - T);
}
