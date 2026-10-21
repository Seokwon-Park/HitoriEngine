#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"

cbuffer FogConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float3 CameraPosition;
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float4 FogColor;
    float FogHeight;
}

Texture2D<float> SceneDepth : register(t0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

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
    float2 NDC = Input.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float Depth = SceneDepth.Load(int3(Input.Position.xy, 0));
    
    //픽셀의 월드 좌표 계산
    float4 WorldPos = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    WorldPos /= WorldPos.w;
    float3 CameraToPixel = WorldPos.xyz - CameraPosition;
    float Distance = length(CameraToPixel);
    
    // Cutoff보다 먼 픽셀은 안개를 적용하지 않음 (0이면 사용 안 함)
    if (FogCutoffDistance > 0.0f && Distance > FogCutoffDistance)
        return float4(0, 0, 0, 0);
    
    //안개가 시작되는 지점보다 가까우면 안개를 적용하지 않음
    if (Distance <= StartDistance)
        return float4(0, 0, 0, 0);
    
    // Fog 시작 위치 계산
    float3 FogStartPos = CameraPosition + normalize(CameraToPixel) * StartDistance;
    
    //안개 시작지점의 밀도
    float RayOriginDensity = FogDensity * exp(-FogHeightFalloff * (FogStartPos.z - FogHeight));
    
    float RayLength = Distance - StartDistance;
    float RayDeltaZ = WorldPos.z - FogStartPos.z;

    // 광선을 따라 높이가 변하면서 밀도도 변하는 걸 반영하는 보정 계수
    //    수평(DeltaZ ≈ 0)이면 밀도가 일정하므로 1
    float k = FogHeightFalloff * RayDeltaZ;
    float HeightFactor = abs(k) > 0.01f ? (1.0f - exp(-k)) / k : 1.0f;

    // 광선 전체에 쌓인 안개량 = 출발점 밀도 × 보정 × 길이
    float LineIntegral = RayOriginDensity * HeightFactor * RayLength;

    //투과율 -> 안개 양, MaxOpacity로 상한
    float FogAmount = 1.0f - exp(-LineIntegral);
    FogAmount = min(FogAmount, FogMaxOpacity);

    // 안개는 톤 매핑 전 HDR 씬에 섞이므로 에디터에서 고른 sRGB 색을 선형으로 바꿔서 쓴다.
    return float4(SRGBToLinear(FogColor.rgb) * FogAmount, FogAmount);
}