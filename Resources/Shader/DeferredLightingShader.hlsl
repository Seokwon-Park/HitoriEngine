#pragma pack_matrix(row_major)
#include "LightData.hlsli"

Texture2D<float4> BaseColorMetallic : register(t0);
Texture2D<float4> NormalRoughness : register(t1);
Texture2D<float> SceneDepth : register(t2);
TextureCube<float4> SkyIrradiance : register(t4);
SamplerState SkySampler : register(s1);

cbuffer DeferredLightConstants : register(b0)
{
    matrix InverseViewProjection;
    float4 CameraPosition; // W: 원근 투영 여부
    float4 CameraForward;
    float4 ViewportSize; // xy: 뷰포트 크기
    float4 AmbientColorIntensity;
    uint4 LightCounts; // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수, w: 렉트 개수
    uint4 LightOffsets; // SceneLights에서 각 종류가 시작되는 위치, 순서는 위와 같음
    float4 SkyColorIntensity;
};

StructuredBuffer<FLightRenderParameters> SceneLights : register(t3);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

void AccumulateLocalLight(inout float3 Diffuse, inout float3 Specular,
    float3 Albedo, float Metallic, float Roughness,
    float3 N, float3 V, float3 L, float3 Radiance)
{
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0f) return;

    Diffuse += Albedo * (1.0f - saturate(Metallic)) * Radiance * NdotL;

    float3 H = L + V;
    float HLengthSquared = dot(H, H);
    if (HLengthSquared <= 1.0e-8f) return;
    H *= rsqrt(HLengthSquared);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, saturate(Metallic));
    float Gloss = 1.0f - saturate(Roughness);
    float Shininess = lerp(2.0f, 128.0f, Gloss * Gloss);
    Specular += F0 * Radiance * pow(saturate(dot(N, H)), Shininess) * NdotL;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float Depth = SceneDepth.Load(int3(Pixel, 0));
    if (Depth >= 1.0f)
        discard; // 배경은 먼저 그린 스카이박스를 유지한다.

    float4 MaterialData = BaseColorMetallic.Load(int3(Pixel, 0));
    float4 NormalData = NormalRoughness.Load(int3(Pixel, 0));
    float3 Albedo = MaterialData.rgb;
    float Metallic = MaterialData.a;
    float Roughness = NormalData.a;
    float3 N = NormalData.xyz * rsqrt(max(dot(NormalData.xyz, NormalData.xyz), 1.0e-8f));
    
    float3 AmbientColor = AmbientColorIntensity.rgb * max(AmbientColorIntensity.a, 0.0f);
    float3 BaseLighting = AmbientColor;
    if (SkyColorIntensity.a > 0.0f)
        BaseLighting += SkyIrradiance.SampleLevel(SkySampler, N, 0).rgb *
            SkyColorIntensity.rgb * SkyColorIntensity.a;
    [loop]
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.z + DirectionalIndex];
        if (Light.Intensity <= 0.0f) continue;
        float3 LightDir = normalize(Light.Direction);
        float3 LightColor = Light.Color * Light.Intensity;
        BaseLighting += LightColor * saturate(dot(N, -LightDir));
    }

    float3 Lighting = Albedo * BaseLighting;
    if (LightCounts.x == 0 && LightCounts.y == 0 && LightCounts.w == 0)
        return float4(Lighting, 1.0f);

    float2 UV = (float2(Pixel) + 0.5f) / ViewportSize.xy;
    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    if (!isfinite(WorldH.w) || abs(WorldH.w) < 1.0e-6f)
        return 0.0f;
    float3 WorldPosition = WorldH.xyz / WorldH.w;
    // 직교 투영의 시선 광선은 평행하므로 시선 방향이 위치에 따라 달라지지 않는다.
    float3 ToCamera = CameraPosition.w > 0.5f
        ? CameraPosition.xyz - WorldPosition : -CameraForward.xyz;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));

    float3 LocalDiffuse = 0.0f;
    float3 LocalSpecular = 0.0f;

    [loop]
    for (uint PointIndex = 0; PointIndex < LightCounts.x; ++PointIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.x + PointIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.Radius * Light.Radius;

        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            continue;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;

        float x = Distance / Light.Radius;
        float Attenuation = pow(saturate(1.0 - x), max(Light.FalloffExponent, 0.0f));

        float3 Radiance = Light.Color * max(Light.Intensity, 0.0f) * Attenuation;
        AccumulateLocalLight(LocalDiffuse, LocalSpecular,
            Albedo, Metallic, Roughness, N, V, L, Radiance);

    }

    [loop]
    for (uint SpotIndex = 0; SpotIndex < LightCounts.y; ++SpotIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.y + SpotIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.Radius * Light.Radius;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            continue;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;
		float ConeCos = dot(-L, normalize(Light.Direction));
		float ConeFade = pow(smoothstep(Light.OuterConeCos, Light.InnerConeCos, ConeCos),
			max(Light.FalloffExponent, 0.0001f));
		float DistanceFactor = saturate(1.0f - Distance / Light.Radius);
		float RadiusFade = DistanceFactor * DistanceFactor;
		float3 Radiance = Light.Color * max(Light.Intensity, 0.0f)
			* ConeFade * RadiusFade;
        AccumulateLocalLight(LocalDiffuse, LocalSpecular,
            Albedo, Metallic, Roughness, N, V, L, Radiance);
    }
    
    [loop]
    for (uint RectIndex = 0; RectIndex < LightCounts.w; ++RectIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.w + RectIndex];
        float3 Position = Light.Position;
        float Radius = Light.Radius;
        float3 Direction = Light.Direction;
        float BarnDoorLength = Light.BarnDoorLength;
        float3 Color = Light.Color;
        float Intensity = Light.Intensity;
        float BarnDoorCos = Light.BarnDoorCos;
        float BarnDoorSin = Light.BarnDoorSin;
        float FalloffExponent = Light.FalloffExponent;
        float3 Right = Light.Right;
        float HalfWidth = Light.HalfWidth;
        float HalfHeight = Light.HalfHeight;

        float3 Up = cross(Direction, Right);
        float3 CenterToSurface = WorldPosition - Position;

        float RectX = clamp(dot(CenterToSurface, Right), -HalfWidth, HalfWidth);
        float RectY = clamp(dot(CenterToSurface, Up), -HalfHeight, HalfHeight);
        float3 ClosestPoint = Position + Right * RectX + Up * RectY;

        float3 ToLight = ClosestPoint - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Radius * Radius;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            continue;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;

        float Attenuation = pow(saturate(1.0f - Distance / max(Radius, 0.001f)), max(FalloffExponent, 0.001f));
        float FrontFace = saturate(dot(-L, Direction));
        if (FrontFace <= 0.0f || Attenuation <= 0.0f)
            continue;

        float BarnDoorMask = 1.0f;
        if (BarnDoorLength > 0.0f)
        {
            float3 EmitterToSurface = WorldPosition - ClosestPoint;
            float EmitterDist = max(length(EmitterToSurface), 0.0001f);
            float ForwardDist = dot(EmitterToSurface, Direction);
            float LateralX = abs(dot(EmitterToSurface, Right));
            float LateralY = abs(dot(EmitterToSurface, Up));
            float SlackX = (ForwardDist * BarnDoorSin - LateralX * BarnDoorCos) / EmitterDist;
            float SlackY = (ForwardDist * BarnDoorSin - LateralY * BarnDoorCos) / EmitterDist;
            BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
        }

        float3 Radiance = Color * max(Intensity, 0.0f) * Attenuation * FrontFace * BarnDoorMask;
        AccumulateLocalLight(LocalDiffuse, LocalSpecular,
            Albedo, Metallic, Roughness, N, V, L, Radiance);
    }
    
    return float4(Lighting + LocalDiffuse + LocalSpecular, 1.0f);
    
}



// GGX 미세면 정반사와 에너지를 보존하는 Lambert 확산광.
//float3 EvaluateSurfaceBRDF(float3 Albedo, float Metallic, float Roughness,
//    float3 N, float3 V, float3 L)
//{
//    const float PI = 3.14159265359f;
//    float NoL = saturate(dot(N, L));
//    float NoV = saturate(dot(N, V));
//    if (NoL <= 0.0f || NoV <= 0.0f) return 0.0f;

//    float3 HalfVector = V + L;
//    float HalfLengthSquared = dot(HalfVector, HalfVector);
//    if (HalfLengthSquared < 1.0e-8f) return 0.0f;
//    float3 H = HalfVector * rsqrt(HalfLengthSquared);
//    float NoH = saturate(dot(N, H));
//    float VoH = saturate(dot(V, H));
//    Metallic = saturate(Metallic);
//    Roughness = clamp(Roughness, 0.045f, 1.0f);

//    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, Metallic);
//    float3 F = F0 + (1.0f - F0) * pow(1.0f - VoH, 5.0f);

//    float Alpha = Roughness * Roughness;
//    float AlphaSquared = Alpha * Alpha;
//    float Denominator = NoH * NoH * (AlphaSquared - 1.0f) + 1.0f;
//    float D = AlphaSquared / max(PI * Denominator * Denominator, 1.0e-12f);
//    float GL = 2.0f * NoL /
//        (NoL + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoL * NoL));
//    float GV = 2.0f * NoV /
//        (NoV + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoV * NoV));
//    float3 Specular = D * GL * GV / max(4.0f * NoL * NoV, 1.0e-6f) * F;
//    float3 Diffuse = (1.0f - F) * (1.0f - Metallic) * Albedo / PI;
//    return (Diffuse + Specular) * NoL;
//}


//float4 mainPS(FPSInput Input) : SV_Target
//{
//    int2 Pixel = int2(Input.Position.xy);
//    float Depth = SceneDepth.Load(int3(Pixel, 0));
//    if (Depth >= 1.0f)
//        discard; // 배경은 먼저 그린 스카이박스를 유지한다.

//    float4 MaterialData = BaseColorMetallic.Load(int3(Pixel, 0));
//    float4 NormalData = NormalRoughness.Load(int3(Pixel, 0));
//    float3 Albedo = MaterialData.rgb;
//    float Metallic = MaterialData.a;
//    float Roughness = NormalData.a;
//    float3 N = NormalData.xyz * rsqrt(max(dot(NormalData.xyz, NormalData.xyz), 1.0e-8f));

//    // 첫 드로우는 기본 조명을 HDR에 쓰고, 두 번째 드로우는 포인트 라이트만 더한다.
//    if (ViewportLightCount.w < 0.5f)
//    {
//        const float3 LightDir = normalize(float3(0.5f, 0.5f, -1.0f));
//        const float3 AmbientColor = float3(0.5f, 0.5f, 0.5f);
//        const float3 LightColor = float3(0.5f, 0.5f, 0.5f);
//        float NdotL = saturate(dot(N, -LightDir));
//        return float4(Albedo * (AmbientColor + LightColor * NdotL), 1.0f);
//    }

//    float2 UV = (float2(Pixel) + 0.5f) / ViewportLightCount.xy;
//    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
//    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
//    if (!isfinite(WorldH.w) || abs(WorldH.w) < 1.0e-6f)
//        return 0.0f;
//    float3 WorldPosition = WorldH.xyz / WorldH.w;
//    // 직교 투영의 시선 광선은 평행하므로 시선 방향이 위치에 따라 달라지지 않는다.
//    float3 ToCamera = CameraPosition.w > 0.5f
//        ? CameraPosition.xyz - WorldPosition : -CameraForward.xyz;
//    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));

//    float3 PointLighting = 0.0f;

//    [loop]
//    for (uint Index = 0; Index < (uint) ViewportLightCount.z; ++Index)
//    {
//        FPointLightData Light = PointLights[Index];
//        float3 ToLight = Light.PositionRadius.xyz - WorldPosition;
//        float DistanceSquared = dot(ToLight, ToLight);
//        float RadiusSquared = Light.PositionRadius.w * Light.PositionRadius.w;
//        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
//            continue;

//        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
//        float3 L = ToLight / Distance;
//        float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
//        float Attenuation = RadiusFade * RadiusFade / (1.0f + DistanceSquared);
//        float3 SurfaceResponse = EvaluateSurfaceBRDF(Albedo, Metallic, Roughness, N, V, L);
//        float3 LightRadiance = Light.ColorIntensity.rgb * Light.ColorIntensity.a * Attenuation;
//        PointLighting += SurfaceResponse * LightRadiance;
//    }

//    return float4(PointLighting, 0.0f);
//}

