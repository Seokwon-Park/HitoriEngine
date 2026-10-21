#pragma pack_matrix(row_major) 
Texture2D<float> SceneDepth : register(t0);

cbuffer FDepthViewConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float3 CameraPosition;
    float VisualizeRange;
    float3 CameraForward;
    float Padding;
};


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

    if (Depth == 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    float4 WorldPos = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    WorldPos /= WorldPos.w;

    // 카메라 앞쪽 축으로 투영한 거리 = z_v (뷰 공간 깊이)
    float ViewDepth = dot(WorldPos.xyz - CameraPosition, CameraForward);

        
        
    float Shade = saturate(ViewDepth / VisualizeRange);
    return float4(Shade.xxx, 1.0f);
}