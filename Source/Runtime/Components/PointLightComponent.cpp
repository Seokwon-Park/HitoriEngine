#include "EnginePCH.h"
#include "PointLightComponent.h"

#include "Engine/LightSceneProxy.h"

#include "Render/LineBatcher.h"

UPointLightComponent::UPointLightComponent() : LightFalloffExponent(1.0f) {}
UPointLightComponent::~UPointLightComponent() {}

FLightSceneProxy* UPointLightComponent::CreateSceneProxy() const
{
    return new FPointLightSceneProxy(this);
}

namespace
{
    constexpr int32 SPHERE_SEGMENT_COUNT = 32;

    void AddWireSphere(
        FLineBatcher* LineBatcher,
        const FVector& Center,
        const FVector& AxisA,
        const FVector& AxisB,
        float Radius)
    {
        FVector Prev = Center + AxisA * Radius;
        const FVector4 Color(0.0f, 248.0f, 255.0f, 255.0f);

        for (int32 i = 1; i <= SPHERE_SEGMENT_COUNT; ++i)
        {
            const float Angle = 2 * PI * i / SPHERE_SEGMENT_COUNT;
            const FVector Point = Center + AxisA * (std::cosf(Angle)*Radius) + AxisB * (std::sinf(Angle)*Radius);
            LineBatcher->AddLine(Prev, Point, Color);
            Prev = Point;
        }
    }
}

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const 
{
    // AttenuationRadius 반경의 원 3개(XY, XZ, YZ)
    if (LineBatcher == nullptr) return;

    const FVector Center = GetWorldLocation();
    float Radius = (GetAttenuationRadius() > 0.0f)? GetAttenuationRadius() : 0.0f;

    AddWireSphere(LineBatcher, Center, FVector(1,0,0), FVector(0,0,1), Radius);
    AddWireSphere(LineBatcher, Center, FVector(0,1,0), FVector(0,0,1), Radius);
    AddWireSphere(LineBatcher, Center, FVector(1,0,0), FVector(0,1,0), Radius);
}
