#pragma pack_matrix(row_major)
cbuffer constants : register(b0)
{
    matrix ViewProj;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = mul(float4(input.position, 1.0f), ViewProj);
    //output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color + float4(0.5f, 0.5f, 0.5f, 0.0f);
}