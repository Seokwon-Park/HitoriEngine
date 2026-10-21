#ifndef COLOR_SPACE_HLSLI
#define COLOR_SPACE_HLSLI

// 선형 워크플로 공용 함수.
// - 8bit/BC 텍스처와 에디터에서 고른 색은 sRGB로 인코딩된 값이다. 조명 계산 전에 선형으로 바꾼다.
// - 씬은 HDR 타깃에 선형 값으로 쌓이고, ToneMapping.hlsl이 마지막에 한 번만 sRGB로 인코딩한다.
// - 노멀맵처럼 "색이 아닌 데이터" 텍스처에는 쓰지 않는다.
// - 확장자가 .hlsli라 RenderResourceManager가 단독 셰이더로 컴파일하지 않는다.

// IEC 61966-2-1 sRGB -> Linear (Math/Color.h의 sRGBToLinearTable과 같은 공식)
float3 SRGBToLinear(float3 C)
{
    C = saturate(C);
    const float3 Low = C / 12.92f;
    const float3 High = pow((C + 0.055f) / 1.055f, 2.4f);
    return lerp(Low, High, step(0.04045f, C));
}

// Linear -> sRGB. 톤 매핑으로 [0,1]에 들어온 값에만 쓴다.
float3 LinearToSRGB(float3 C)
{
    C = saturate(C);
    const float3 Low = C * 12.92f;
    const float3 High = 1.055f * pow(C, 1.0f / 2.4f) - 0.055f;
    return lerp(Low, High, step(0.0031308f, C));
}

#endif
