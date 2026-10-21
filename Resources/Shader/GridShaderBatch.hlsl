#pragma pack_matrix(row_major)
// CPU는 월드 좌표를 보내고 기존 Grid와 같은 행렬 규약으로 GPU에서 VP를 적용한다.
cbuffer GridLineCB : register(b0)
{
    float4x4 ViewProj;
    float2 ViewportSize;
    float2 Padding;
    float4 FadeOriginAndRadius;
};

// 월드 선의 양 끝점과 끝점 선택·좌우 방향·반두께 및 색상을 담는다.
struct VS_INPUT
{
    float3 start : POSITION0;
    float3 end : POSITION1;
    float3 shape : TEXCOORD0;
    float4 color : COLOR;
};

// Clip 위치와 화면 공간 가장자리 거리를 전달해 두께와 투영 깊이를 보존한다.
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    noperspective float2 edge : TEXCOORD0;
    float4 color : COLOR;
    float3 worldPosition : TEXCOORD1;
};

// 동차 평면의 부호 있는 거리로 선의 유효 매개변수 구간을 자른다.
bool ClipLinePlane(float a, float b, inout float first, inout float last)
{
    if (a < 0.0f && b < 0.0f) return false;
    if ((a < 0.0f) != (b < 0.0f))
    {
        float t = a / (a - b);
        if (a < 0.0f) first = max(first, t);
        else last = min(last, t);
    }
    return first <= last;
}

// 월드 끝점에 VP를 곱해 클리핑하고 화면 픽셀 두께만 Clip XY에 더한다.
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.pos = float4(0, 0, -1, 1);
    float4 a = mul(float4(input.start, 1), ViewProj);
    float4 b = mul(float4(input.end, 1), ViewProj);
    if (!all(isfinite(a)) || !all(isfinite(b))) return output;
    float first = 0, last = 1;
    float2 margin = 1.0f + 2.0f * (input.shape.z + 1.0f) / ViewportSize;
    if (!ClipLinePlane(a.w - 1.0e-6f, b.w - 1.0e-6f, first, last)) return output;
    if (!ClipLinePlane(a.z, b.z, first, last)) return output;
    if (!ClipLinePlane(a.w - a.z, b.w - b.z, first, last)) return output;
    if (!ClipLinePlane(margin.x * a.w + a.x, margin.x * b.w + b.x, first, last)) return output;
    if (!ClipLinePlane(margin.x * a.w - a.x, margin.x * b.w - b.x, first, last)) return output;
    if (!ClipLinePlane(margin.y * a.w + a.y, margin.y * b.w + b.y, first, last)) return output;
    if (!ClipLinePlane(margin.y * a.w - a.y, margin.y * b.w - b.y, first, last)) return output;
    float4 delta = b - a;
    b = a + delta * last;
    a = a + delta * first;
    if (a.w <= 0 || b.w <= 0) return output;
    float2 direction = (b.xy / b.w - a.xy / a.w) * 0.5f * ViewportSize;
    float lineLength = length(direction);
    if (lineLength < 0.01f) return output;
    float radius = input.shape.z + 0.75f;
    float2 offset = float2(-direction.y, direction.x) / lineLength * radius * 2.0f / ViewportSize;
    float4 position = input.shape.x < 0.5f ? a : b;
    position.xy += offset * input.shape.y * position.w;
    output.pos = position;
    output.edge = float2(input.shape.y * radius, input.shape.z);
    output.color = input.color;
    // 잘린 끝점의 월드 위치를 보간해 거리 페이드를 실제 월드 선에 적용한다.
    output.worldPosition = lerp(input.start, input.end, input.shape.x < 0.5f ? first : last);
    return output;
}

// 선 중심은 일정 두께로 유지하고 가장자리 한 픽셀을 부드럽게 혼합한다.
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float coverage = 1.0f - smoothstep(input.edge.y - 0.5f, input.edge.y + 0.5f, abs(input.edge.x));
    // 직교는 반경 0으로 끄고, 원근은 기존 Grid와 같은 중심·반경에서 축도 사라지게 한다.
    if (FadeOriginAndRadius.w > 0.0f)
        coverage *= smoothstep(1.0f, 0.5f,
            length(input.worldPosition - FadeOriginAndRadius.xyz) / FadeOriginAndRadius.w);
    clip(coverage - 0.001f);
    return float4(input.color.rgb, input.color.a * coverage);
}
