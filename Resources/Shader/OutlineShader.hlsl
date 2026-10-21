#pragma pack_matrix(row_major)
// View별 행렬과 렌더 타깃 픽셀 크기·Outline 확장량을 전달한다.
cbuffer constants : register(b0)
{
    matrix World;
    matrix NormalMatrix;
    matrix ViewProjection;
    float4 ViewportAndThickness;
};

// 기존 위치·색상 Mesh 정점 입력 형식을 유지한다.
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};


// 확장된 Clip 위치와 선택 Outline 색상을 전달한다.
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// 중심에서 바깥으로 향하는 투영 방향을 픽셀 공간에서 정규화해 일정 두께로 확장한다.
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    const matrix worldViewProjection = mul(World, ViewProjection);
    
    float4 clipPosition = mul(float4(input.position, 1.0f), worldViewProjection);
    float4 worldNormal = mul(float4(input.normal, 0.0f), NormalMatrix);
    const float4 clipDirection = mul(worldNormal, ViewProjection);
    const float2 viewportSize = max(ViewportAndThickness.xy, float2(1.0f, 1.0f));
    
    // 원근 나눗셈의 방향 미분을 사용하고 W 보정으로 원근·직교 모두 픽셀 폭을 유지한다.
    float2 pixelDirection = (clipDirection.xy * clipPosition.w - clipPosition.xy * clipDirection.w) * viewportSize;
    
    const float lengthSquared = dot(pixelDirection, pixelDirection);
    
    if (clipPosition.w > 1.0e-6f && lengthSquared > 1.0e-12f)
    {
        pixelDirection *= rsqrt(lengthSquared);
        clipPosition.xy += pixelDirection * (2.0f * ViewportAndThickness.z / viewportSize) * clipPosition.w;
    }
    output.position = clipPosition;
    output.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    return output;
}

// 선택 Outline의 노란색을 출력한다.
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
