Texture2D<float4> Panorama : register(t0);
TextureCube<float4> Environment : register(t1);
RWTexture2DArray<float4> OutputCube : register(u0);
SamplerState LinearSampler : register(s0);

cbuffer SkyComputeConstants : register(b0)
{
    uint OutputSize;
    uint PanoramaHeight;
    uint bSourceIsLinear;
    uint SampleCount;
};

static const float PI = 3.14159265358979323846f;
static const float GoldenAngle = 2.39996322972865332f;

float3 CubeDirection(uint Face, float2 UV)
{
    float2 P = UV * 2.0f - 1.0f;
    if (Face == 0) return normalize(float3( 1.0f, -P.y, -P.x));
    if (Face == 1) return normalize(float3(-1.0f, -P.y,  P.x));
    if (Face == 2) return normalize(float3( P.x,  1.0f,  P.y));
    if (Face == 3) return normalize(float3( P.x, -1.0f, -P.y));
    if (Face == 4) return normalize(float3( P.x, -P.y,  1.0f));
    return normalize(float3(-P.x, -P.y, -1.0f));
}

float3 SRGBToLinear(float3 Color)
{
    Color = max(Color, 0.0f);
    float3 Low = Color / 12.92f;
    float3 High = pow((Color + 0.055f) / 1.055f, 2.4f);
    return lerp(Low, High, step(0.04045f, Color));
}

[numthreads(8, 8, 1)]
void BuildEnvironmentCube(uint3 Texel : SV_DispatchThreadID)
{
    if (Texel.x >= OutputSize || Texel.y >= OutputSize || Texel.z >= 6) return;
    float2 UV = (float2(Texel.xy) + 0.5f) / OutputSize;
    float3 Direction = CubeDirection(Texel.z, UV);
    float2 PanoramaUV = float2(atan2(Direction.y, Direction.x) / (2.0f * PI) + 0.5f,
        acos(clamp(Direction.z, -1.0f, 1.0f)) / PI);
    PanoramaUV.y = clamp(PanoramaUV.y, 0.5f / PanoramaHeight,
        1.0f - 0.5f / PanoramaHeight);
    float3 Color = Panorama.SampleLevel(LinearSampler, PanoramaUV, 0).rgb;
    if (bSourceIsLinear == 0) Color = SRGBToLinear(Color);
    OutputCube[Texel] = float4(max(Color, 0.0f), 1.0f);
}

[numthreads(8, 8, 1)]
void BuildIrradianceCube(uint3 Texel : SV_DispatchThreadID)
{
    if (Texel.x >= OutputSize || Texel.y >= OutputSize || Texel.z >= 6) return;
    float2 UV = (float2(Texel.xy) + 0.5f) / OutputSize;
    float3 Normal = CubeDirection(Texel.z, UV);
    float3 Sum = 0.0f;
    [loop]
    for (uint Index = 0; Index < SampleCount; ++Index)
    {
        float Z = 1.0f - 2.0f * (Index + 0.5f) / SampleCount;
        float Radius = sqrt(max(0.0f, 1.0f - Z * Z));
        float Phi = Index * GoldenAngle;
        float3 L = float3(Radius * cos(Phi), Radius * sin(Phi), Z);
        Sum += Environment.SampleLevel(LinearSampler, L, 0).rgb * max(dot(Normal, L), 0.0f);
    }
    // 구면 적분의 4π/N을 Lambert 확산광의 π로 나눈다.
    OutputCube[Texel] = float4(Sum * (4.0f / SampleCount), 1.0f);
}
