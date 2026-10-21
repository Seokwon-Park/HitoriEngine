#pragma pack_matrix(row_major)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer WorldConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float2 TextureFlags; // x: 노멀맵 인코딩, y: 노멀맵 Y 반전
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
	float2 UV : TEXCOORD;
	float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldNormal : NORMAL;
    float4 WorldTangent : TANGENT;
    float2 UV : TEXCOORD0;
};

struct GBUFFER_OUTPUT
{
    float4 BaseColorMetallic : SV_Target0;
    float4 NormalRoughness : SV_Target1;
};

Texture2D ColorTexture : register(t0);
Texture2D NormalTexture : register(t1);
SamplerState ColorSampler : register(s0);

float3 ResolveWorldNormal(float3 WorldNormal, float4 WorldTangent, float3 MapNormal)
{
    float3 N = normalize(WorldNormal);
    float3 T = WorldTangent.xyz - N * dot(N, WorldTangent.xyz);
    T *= rsqrt(max(dot(T, T), 1.0e-8f));
    float3 B = cross(N, T) * WorldTangent.w;
    return normalize(T * MapNormal.x + B * MapNormal.y + N * MapNormal.z);
}

float3 DecodeMapNormal(float3 Sample, float Encoding)
{
    if (Encoding > 1.5f)
    {
        float2 XY = Encoding > 2.5f ? Sample.xy : Sample.xy * 2.0f - 1.0f;
        return float3(XY, sqrt(saturate(1.0f - dot(XY, XY))));
    }
    return Sample * 2.0f - 1.0f;
}

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);

    Output.Position = mul(WorldPosition, VP);
    Output.WorldNormal = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;

    // CPU가 구한 로컬 공간 부호에 월드 변환의 거울 반전만 반영한다.
    float Handedness = Input.Tangent.w * (determinant((float3x3)World) < 0.0f ? -1.0f : 1.0f);
    Output.WorldTangent = float4(T, Handedness);
    Output.UV = Input.UV;
    return Output;
}

GBUFFER_OUTPUT mainPS(PS_INPUT Input)
{
    GBUFFER_OUTPUT Output;

    float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV + UVOffset);
    // 색 텍스처는 UNORM으로 읽히므로 sRGB 값을 선형 조명 공간으로 변환한다.
    float3 LinearAlbedo = pow(max(TextureColor.rgb, 0.0f), 2.2f) * BaseColor.rgb;
    float3 Normal = normalize(Input.WorldNormal);
    if (TextureFlags.x > 0.5f)
    {
        float3 MapNormal = DecodeMapNormal(NormalTexture.Sample(ColorSampler, Input.UV + UVOffset).xyz, TextureFlags.x);
        if (TextureFlags.y > 0.5f) MapNormal.y = -MapNormal.y;
        Normal = ResolveWorldNormal(Input.WorldNormal, Input.WorldTangent, MapNormal);
    }

    // 현재 UMaterial에는 Metallic/Roughness 값이 없어 임시 상수를 쓴다.
    Output.BaseColorMetallic = float4(LinearAlbedo, 0.0f);
    Output.NormalRoughness = float4(Normal, 0.8f);
    return Output;
}
