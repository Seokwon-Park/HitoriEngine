#pragma pack_matrix(row_major)
cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

struct VS_INPUT
{
	float3 position : POSITION;
    float2 uv : TEXCOORD0;
	float4 color : COLOR; 

};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
	float4 color : COLOR;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;
    
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.color = float4(input.position, 1.0f);
    output.uv = input.uv;
	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = Texture.Sample(Sampler, input.uv);
    
    return color;
}
