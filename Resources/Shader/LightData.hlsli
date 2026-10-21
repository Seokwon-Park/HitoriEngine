#ifndef LIGHT_DATA_INCLUDED
#define LIGHT_DATA_INCLUDED

// 모든 광원이 공유하는 GPU 원소. C++ SceneManagement.h의 FLightRenderParameters와 바이트 단위로 같다.
#pragma pack_matrix(row_major)

// ELightType과 같은 값이다.
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT  2
#define LIGHT_TYPE_RECT  3

struct FLightRenderParameters
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
    float3 Direction; // 방향광: 빛의 진행 방향, Spot·Rect: 빛을 내는 방향
    float InnerConeCos;
    float3 Right; // Rect 전용
    float FalloffExponent;
    float OuterConeCos;
    float HalfWidth;
    float HalfHeight;
    uint Type;
    float BarnDoorCos; // Rect 전용: 발광면 가장자리의 문(Barn Door)
    float BarnDoorSin;
    float BarnDoorLength;
    int ShadowMapChannel;
};

#endif
