#pragma pack_matrix(row_major)

// 기존 메시 경로의 시점·오브젝트 상수 슬롯을 공유한다.
cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer WorldConstants : register(b2)
{
    matrix World;
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

float4 mainVS(VS_INPUT Input) : SV_POSITION
{
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    return mul(WorldPosition, VP);
}

void mainPS()
{
    
}
