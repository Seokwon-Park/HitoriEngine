#include "EnginePCH.h"
#include "RectLightComponent.h"
#include "Render/LineBatcher.h"

#include "Engine/LightSceneProxy.h"

URectLightComponent::URectLightComponent() {}
URectLightComponent::~URectLightComponent() {}

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

void URectLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (LineBatcher == nullptr) return;

	FTransform WorldTransform;
	WorldTransform.Location = GetWorldLocation();
	WorldTransform.Rotation = GetWorldRotation();

	const FVector Center = WorldTransform.Location;
	const FVector Forward = WorldTransform.GetForward();   // +X, 빛이 나가는 방향
	const FVector Right = WorldTransform.GetRight();
	const FVector Up = WorldTransform.GetUp();

	const float HW = std::max(HalfWidth, 0.0f);
	const float HH = std::max(HalfHeight, 0.0f);
	const float Angle = FMath::DegreesToRadians(FMath::Clamp(BarnDoorAngle, 0.0f, 90.0f));
	const float Len = std::max(BarnDoorLength, 0.0f);

	// 발광면 (우상, 좌상, 좌하, 우하)
    const FVector C0 = Center + Right * HW + Up * HH;
    const FVector C1 = Center - Right * HW + Up * HH;
    const FVector C2 = Center - Right * HW - Up * HH;
    const FVector C3 = Center + Right * HW - Up * HH;

    const FVector4 Color = FColor(255.0f, 246.0f, 0.0f, 255.0f).ToVector4();

    // 개구부 사각형: 앞으로 F만큼 나가고, 사방으로 S만큼 벌어진다
    const float F = std::cosf(Angle) * Len;     // 앞으로 나간 양
    const float S = std::sinf(Angle) * Len;     // 바깥으로 벌어진 양
    const FVector AC = Center + Forward * F;

    const FVector A0 = AC + Right * (HW + S) + Up * (HH + S);
    const FVector A1 = AC - Right * (HW + S) + Up * (HH + S);
    const FVector A2 = AC - Right * (HW + S) - Up * (HH + S);
    const FVector A3 = AC + Right * (HW + S) - Up * (HH + S);

    // 사각형
    LineBatcher->AddLine(C0, C1, Color);
    LineBatcher->AddLine(C1, C2, Color);
    LineBatcher->AddLine(C2, C3, Color);
    LineBatcher->AddLine(C3, C0, Color);

    // 개구부 사각형
    LineBatcher->AddLine(A0, A1, Color);
    LineBatcher->AddLine(A1, A2, Color);
    LineBatcher->AddLine(A2, A3, Color);
    LineBatcher->AddLine(A3, A0, Color);

    // 발광면 모서리와 개구부 모서리 연결
    LineBatcher->AddLine(C0, A0, Color);
    LineBatcher->AddLine(C1, A1, Color);
    LineBatcher->AddLine(C2, A2, Color);
    LineBatcher->AddLine(C3, A3, Color);
    
    // AttenuationRadius 반경의 원 3개(XY, XZ, YZ)
    {
    if (LineBatcher == nullptr) return;

    const FVector Center = GetWorldLocation();
    float Radius = (GetAttenuationRadius() > 0.0f)? GetAttenuationRadius() : 0.0f;

    AddWireSphere(LineBatcher, Center, FVector(1,0,0), FVector(0,0,1), Radius);
    AddWireSphere(LineBatcher, Center, FVector(0,1,0), FVector(0,0,1), Radius);
    AddWireSphere(LineBatcher, Center, FVector(1,0,0), FVector(0,1,0), Radius);
    }
}

FLightSceneProxy* URectLightComponent::CreateSceneProxy() const
{
    return new FRectLightSceneProxy(this);
}
