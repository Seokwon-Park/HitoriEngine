#pragma pack_matrix(row_major)

#include "ColorSpace.hlsli"
#include "LightData.hlsli"
#include "LightGridCommon.hlsli"

// 조명 모델은 C++(FRenderResourceManager::InitPipelineStates)이 매크로로 하나만 넘긴다.
// 아무 매크로 없이 컴파일될 때(기존 머티리얼이 쓰는 기본 키)만 Lambert를 켠다.
#if !defined(LIGHTING_MODEL_UNLIT) && !defined(LIGHTING_MODEL_GOURAUD) && \
    !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && \
    !defined(VIEW_MODE_WORLD_NORMAL)
#define LIGHTING_MODEL_LAMBERT 1
#endif

// 아래 cbuffer들은 ObjectConstants.h / LightRenderData.h와 바이트 단위로 같아야 한다.
cbuffer PerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix WorldInverseTranspose; // (World^-1)^T
    float4 ObjectFlags;
};

cbuffer Lighting : register(b1)
{
    float4 AmbientColorIntensity; // rgb: 선형 색상, a: 광도
    float4 CameraPosition; // Specular의 V = normalize(CameraPosition - WorldPos)
    uint4 LightCounts; // x: 포인트, y: 스포트, z: 방향광, w: 렉트 개수
    uint4 LightOffsets; // SceneLights에서 각 종류가 시작되는 위치, 순서는 위와 같음
    float4 SkyColorIntensity; // rgb: Sky Light 색상, a: 세기 (0이면 비활성)
};

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // x: 노멀맵 Y 반전, y: 예약, z: 컬러 텍스처 사용, w: 노멀맵 인코딩
    float4 UVTransform; // xy: 이동량, zw: 배율
    float4 SpecularColorShininess; // rgb: 정반사 색상, w: Blinn-Phong 광택 지수
};

// 방향광, 포인트, 스포트, 렉트 순으로 이어져 있다. 포인트부터 렉트까지가 로컬 광원 구간이다.
StructuredBuffer<FLightRenderParameters> SceneLights : register(t8);
// CellLightIndices(t9)와 LightGridConstants(b3)는 LightGridCommon.hlsli에 있다.
TextureCube<float4> SkyIrradiance : register(t11);
Texture2D<float4> ShadowMask : register(t13);
SamplerState SkySampler : register(s1);

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);
SamplerState MaterialSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT; // w = 바이탄젠트 부호
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

// VS가 출력하고 PS가 입력으로 받는다.
struct VS_OUTPUT
{
    float4 SvPosition : SV_POSITION; // VS: 클립 공간 / PS: 픽셀 좌표(xy), 깊이(z)
    float3 WorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : COLOR1;   // 정점에서 미리 계산한 Diffuse 조명
#endif
};

// Diffuse와 Specular를 담는 구조체
struct FLightContribution
{
    float3 Diffuse;
    float3 Specular;
};

uint GetNumLocalLights()
{
    return LightCounts.x + LightCounts.y + LightCounts.w;
}

// 원뿔 감쇠량. 0(Outer 밖) ~ 1(Inner 안)
float GetSpotConeAttenuation(FLightRenderParameters Light, float3 L)
{
    return smoothstep(Light.OuterConeCos, Light.InnerConeCos, dot(-L, Light.Direction));
}

// 거리 감쇠량을 돌려주고, 표면에서 빛을 향하는 단위 벡터 L을 OutL로 내보낸다.
float GetDistanceAttenuation(float3 LightPos, float AttenuationRadius, float FalloffExponent, float3 WorldPos, out float3 OutL)
{
    const float3 ToLight = LightPos - WorldPos; // 픽셀의 월드 좌표에서 빛을 향하는 벡터
    const float Distance = length(ToLight);
    OutL = ToLight / max(Distance, 0.0001f);
    // 거리가 Radius에 가까울수록 0으로
    return pow(saturate(1.0f - Distance / max(AttenuationRadius, 0.001f)), max(FalloffExponent, 0.001f));
}

// 라이트 하나의 Diffuse와 Specular를 구한다. Radiance = Color * Intensity * Attenuation (* ConeAttenuation)
FLightContribution SurfaceShading(float3 N, float3 L, float3 V, float3 Radiance, float InShininess)
{
    const float NdotL = saturate(dot(normalize(N), normalize(L)));
    FLightContribution Contribution = (FLightContribution) 0;
    Contribution.Diffuse = NdotL;

#if LIGHTING_MODEL_PHONG
    // Blinn Specular
    const float3 H = normalize(L + V);
    if (dot(N, L) > 0)
    {
        Contribution.Specular = pow(saturate(dot(normalize(N), H)), max(InShininess, 1.f));
    }
#endif
    Contribution.Diffuse *= Radiance;
    Contribution.Specular *= Radiance;
    return Contribution;
}

// 표면에 들어오는 빛의 양만 돌려준다. 표면색(albedo)은 호출한 쪽에서 곱한다.
// Sky Light가 있으면 C++이 Ambient를 0으로 보내므로 둘을 더하면 된다.
float3 GetAmbientLighting(float3 N)
{
    float3 Result = AmbientColorIntensity.rgb * max(AmbientColorIntensity.a, 0.0f);
    if (SkyColorIntensity.a > 0.0f)
        Result += SkyIrradiance.SampleLevel(SkySampler, N, 0).rgb * SkyColorIntensity.rgb * SkyColorIntensity.a;
    return Result;
}

// Directional Light의 영향을 구한다
FLightContribution CalcDirectionalLight(FLightRenderParameters Light, float3 N, float3 V, float InShininess)
{
    return SurfaceShading(N, -Light.Direction, V, Light.Color * Light.Intensity, InShininess);
}

// 해당 WorldPos에서 PointLight 하나의 영향(밝기)을 구한다
FLightContribution CalcPointLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    float3 L;
    const float Attenuation = GetDistanceAttenuation(Light.Position, Light.Radius, Light.FalloffExponent, WorldPos, L);
    return SurfaceShading(N, L, V, Light.Color * Light.Intensity * Attenuation, InShininess);
}

// Spot은 Point에서 원뿔 감쇠만 곱하면 된다
FLightContribution CalcSpotLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    float3 L;
    const float Attenuation = GetDistanceAttenuation(Light.Position, Light.Radius, Light.FalloffExponent, WorldPos, L);
    return SurfaceShading(N, L, V, Light.Color * Light.Intensity * Attenuation * GetSpotConeAttenuation(Light, L), InShininess);
}

FLightContribution CalcRectLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    const float3 Up = cross(Light.Direction, Light.Right); // Z축
    const float3 CenterToSurface = WorldPos - Light.Position;

    // 1) 사각형 위에서 표면과 가장 가까운 점 찾기
    const float RectX = clamp(dot(CenterToSurface, Light.Right), -Light.HalfWidth, Light.HalfWidth); // 사각형 가로 좌표
    const float RectY = clamp(dot(CenterToSurface, Up), -Light.HalfHeight, Light.HalfHeight); // 사각형 세로 좌표
    const float3 ClosestPoint = Light.Position + Light.Right * RectX + Up * RectY;

    // 2) 가장 가까운 점을 점광원처럼 취급
    float3 L; // 표면 -> 빛
    const float Attenuation = GetDistanceAttenuation(ClosestPoint, Light.Radius, Light.FalloffExponent, WorldPos, L);

    // 3) 면의 앞쪽만 비춘다 (뒤쪽이면 0)
    const float FrontFace = saturate(dot(-L, Light.Direction));

    // 4) 문 안쪽(프러스텀)만 통과
    float BarnDoorMask = 1.0f;

    // Door Length가 0이면 그냥 rect (전방향)
    if (Light.BarnDoorLength > 0.0f)
    {
        const float3 EmitterToSurface = WorldPos - ClosestPoint; // 발광면에서 표면으로
        const float Distance = max(length(EmitterToSurface), 0.0001f);
        const float ForwardDist = dot(EmitterToSurface, Light.Direction);
        const float LateralX = abs(dot(EmitterToSurface, Light.Right));
        const float LateralY = abs(dot(EmitterToSurface, Up));
        // 옆으로 벌어진 각도가 frustum 안인가? (양수면 안쪽)
        const float SlackX = (ForwardDist * Light.BarnDoorSin - LateralX * Light.BarnDoorCos) / Distance;
        const float SlackY = (ForwardDist * Light.BarnDoorSin - LateralY * Light.BarnDoorCos) / Distance;
        // Slack이 −0.05보다 작으면 0
        // x,y 모두 닿아야 빛이 닿으므로 곱으로 마스크 만들기
        BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
    }

    const float3 Radiance = Light.Color * Light.Intensity * Attenuation * FrontFace * BarnDoorMask;
    return SurfaceShading(N, L, V, Radiance, InShininess);
}

// Type에 따라 Point / Spot / Rect로 나눠 계산한다
FLightContribution CalcLocalLight(FLightRenderParameters Light, float3 N, float3 V, float3 WorldPos, float InShininess)
{
    FLightContribution Result;
    if (Light.Type == LIGHT_TYPE_SPOT)
        Result = CalcSpotLight(Light, N, V, WorldPos, InShininess);
    else if (Light.Type == LIGHT_TYPE_RECT)
        Result = CalcRectLight(Light, N, V, WorldPos, InShininess);
    else
        Result = CalcPointLight(Light, N, V, WorldPos, InShininess);
    return Result;
}

// Directional + 로컬 라이트의 Diffuse/Specular를 모두 더한다. Ambient는 포함하지 않는다.
// bUseLightGrid = true면 SvPosition이 속한 셀의 라이트만, false면 전체 로컬 라이트를 순회한다.
// VS(Gouraud)는 픽셀 위치가 없어 셀을 쓸 수 없으므로 false로 호출한다.
FLightContribution AccumulateDirectLighting(float3 N, float3 V, float3 WorldPos, float InShininess, float4 SvPosition, bool bUseLightGrid)
{
    FLightContribution Total = (FLightContribution) 0;
    float4 ShadowFactors = 1.0f;
    
    if (bUseLightGrid)
        ShadowFactors = ShadowMask.Load(int3((int2) SvPosition.xy, 0));
    
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
    {
        const FLightRenderParameters Light = SceneLights[LightOffsets.z + DirectionalIndex];
        FLightContribution LightContribution = CalcDirectionalLight(SceneLights[LightOffsets.z + DirectionalIndex], N, V, InShininess);
        
        if (Light.ShadowMapChannel >= 0 && ObjectFlags.x > 0.5f)
        {
            const float Shadow = ShadowFactors[Light.ShadowMapChannel];
            LightContribution.Diffuse *= Shadow;
            LightContribution.Specular *= Shadow;
        }
        
        Total.Diffuse += LightContribution.Diffuse;
        Total.Specular += LightContribution.Specular;
    }

    uint LightCount = GetNumLocalLights();
    uint CellOffset = 0;
    bool bUseCellList = false;
    if (bUseLightGrid && GridMode != LIGHT_GRID_MODE_OFF)
    {
        CellOffset = GetLightGridCellOffset((uint2) SvPosition.xy, SvPosition.z);
        const uint CellLightCount = CellLightIndices[CellOffset];
        // 셀이 넘쳤으면(상한 이상) 목록이 잘렸으므로 전체 순회로 돌아간다
        if (CellLightCount < MAX_LIGHTS_PER_CELL)
        {
            LightCount = CellLightCount;
            bUseCellList = true;
        }
    }

    for (uint Index = 0; Index < LightCount; ++Index)
    {
        const uint LightIndex = bUseCellList ? CellLightIndices[CellOffset + 1 + Index] : Index;
        const FLightRenderParameters Light = SceneLights[LightOffsets.x + LightIndex];
        FLightContribution LightContribution = CalcLocalLight(SceneLights[LightOffsets.x + LightIndex], N, V, WorldPos, InShininess);
        
        if (Light.ShadowMapChannel >= 0 && ObjectFlags.x > 0.5f)
        {
            const float Shadow = ShadowFactors[Light.ShadowMapChannel];
            LightContribution.Diffuse *= Shadow;
            LightContribution.Specular *= Shadow;
        }
        Total.Diffuse += LightContribution.Diffuse;
        Total.Specular += LightContribution.Specular;
    }

    return Total;
}

// 노멀맵 디코딩. Encoding: 0 없음, 1 RGB, 2 BC5(0~1), 3 BC5(-1~1)
float3 DecodeMapNormal(float3 Sample, float Encoding)
{
    if (Encoding > 1.5f)
    {
        const float2 XY = Encoding > 2.5f ? Sample.xy : Sample.xy * 2.0f - 1.0f;
        return float3(XY, sqrt(saturate(1.0f - dot(XY, XY))));
    }
    return Sample * 2.0f - 1.0f;
}

VS_OUTPUT mainVS(VS_INPUT Input)
{
    VS_OUTPUT Output;

    const float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    Output.SvPosition = mul(mul(WorldPos, View), Projection);
    Output.WorldPos = WorldPos.xyz;

    // 노멀은 역전치로, 탄젠트는 표면을 따라 놓인 방향이라 World로 변환한다.
    // World가 뒤집혀 있으면(행렬식 < 0) 바이탄젠트 부호도 뒤집는다.
    Output.Normal = mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz;
    Output.Tangent = float4(mul(float4(Input.Tangent.xyz, 0.0f), World).xyz, Input.Tangent.w * (determinant((float3x3) World) < 0.0f ? -1.0f : 1.0f));

    Output.UV = Input.UV * UVTransform.zw + UVTransform.xy;
    Output.Color = Input.Color;
#if LIGHTING_MODEL_GOURAUD
    const float3 N = normalize(Output.Normal);
    const float3 V = normalize(CameraPosition.xyz - Output.WorldPos);
    const FLightContribution Contribution = AccumulateDirectLighting(N, V, Output.WorldPos, 1, Output.SvPosition, false);
    Output.VertexDiffuse = GetAmbientLighting(N) + Contribution.Diffuse;
#endif
    return Output;
}

float4 mainPS(VS_OUTPUT Input) : SV_TARGET
{
    const float2 UV = Input.UV;
    // 씬은 HDR 타깃에 선형으로 쌓이므로 sRGB로 인코딩된 디퓨즈 텍스처를 선형으로 바꾼다.
    // 텍스처가 없는 머티리얼은 정점 색을 쓴다.
    float4 SurfaceColor = Input.Color;
    if (SurfaceParams.z > 0.5f)
    {
        const float4 TexColor = DiffuseTexture.Sample(MaterialSampler, UV);
        SurfaceColor = float4(SRGBToLinear(TexColor.rgb), TexColor.a);
    }
    float4 Albedo = SurfaceColor * BaseColor;
#if EDITOR_ARROW_SHADER
    // 에디터 화살표는 톤 매핑 이후 LDR 타깃에 그려지므로 sRGB 그대로 둔다.
    Albedo = (SurfaceParams.z > 0.5f ? DiffuseTexture.Sample(MaterialSampler, UV) : Input.Color) * BaseColor;
#endif

    float3 N = normalize(Input.Normal);
    // TBN: 노멀맵의 탄젠트 공간 노멀을 월드 공간으로 옮긴다.
    if (SurfaceParams.w > 0.5f)
    {
        const float3 VertexNormal = N;
        const float3 T = normalize(Input.Tangent.xyz - VertexNormal * dot(VertexNormal, Input.Tangent.xyz));
        const float3 B = cross(VertexNormal, T) * (Input.Tangent.w < 0.0f ? -1.0f : 1.0f);
        float3 TangentNormal = DecodeMapNormal(NormalTexture.Sample(MaterialSampler, UV).xyz, SurfaceParams.w);
        if (SurfaceParams.x > 0.5f)
            TangentNormal.y = -TangentNormal.y;
        N = normalize(T * TangentNormal.x + B * TangentNormal.y + VertexNormal * TangentNormal.z);
    }

#if LIGHTING_MODEL_UNLIT
    return Albedo;
#elif VIEW_MODE_WORLD_NORMAL
    return float4(N * 0.5f + 0.5f, 1.0f);
#else
    float3 DiffuseLighting = { 0.0f, 0.0f, 0.0f };
    float3 SpecularLighting = { 0.0f, 0.0f, 0.0f };

#if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    const float3 V = normalize(CameraPosition.xyz - Input.WorldPos);
    const FLightContribution Contribution = AccumulateDirectLighting(N, V, Input.WorldPos, SpecularColorShininess.w, Input.SvPosition, true);
    DiffuseLighting = GetAmbientLighting(N) + Contribution.Diffuse;
    SpecularLighting = Contribution.Specular;
#elif LIGHTING_MODEL_GOURAUD
    DiffuseLighting = Input.VertexDiffuse;
#endif

    return float4(Albedo.rgb * DiffuseLighting + SpecularLighting + Emissive.rgb, Albedo.a);
#endif
}
