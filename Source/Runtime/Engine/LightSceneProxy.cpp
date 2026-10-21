#include "EnginePCH.h"
#include "LightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"

namespace
{
	static float ComputeAccumulatedScale(float Exponent, int32 CascadeIndex, int32 CascadeCount)
	{
		if (CascadeIndex <= 0) return 0.0f;
		float CurrentScale = 1.0f;
		// 모든 캐스케이드 가중치의 합
		float TotalScale = 0.0f;
		// CascadeIndex 앞에 있는 캐스케이드 가중치의 합
		float Ret = 0.0f;
		for (int32 i = 0; i < CascadeCount; ++i)
		{
			if (i < CascadeIndex) Ret += CurrentScale;
			TotalScale += CurrentScale;
			CurrentScale *= Exponent;
		}
		return Ret / TotalScale;
	}
}

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(const UDirectionalLightComponent* InComponent)
	:FLightSceneProxy(InComponent)
{
	if (!InComponent) return;
	DynamicShadowDistanceMovableLight = InComponent->DynamicShadowDistanceMovableLight;
	DynamicShadowCascades = InComponent->DynamicShadowCascades;
	CascadeDistributionExponent = InComponent->CascadeDistributionExponent;
	CascadeTransitionFraction = InComponent->CascadeTransitionFraction;
	LightSourceAngle = InComponent->LightSourceAngle;
}

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);

	FTransform Transform;
	Transform.Rotation = Dir->GetWorldRotation();
	const FLinearColor Color(Dir->GetLightColor());

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Direction = Transform.GetForward();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = Dir->GetIntensity();
}

uint32 FDirectionalLightSceneProxy::GetNumViewDependentWholeSceneShadows(const FSceneView& View) const
{
	// 거리 0이면 동적 그림자 끔 (언리얼 주석: "A value of 0 disables the dynamic shadow")
	return DynamicShadowDistanceMovableLight > 0.0f ? DynamicShadowCascades : 0;
}

bool FDirectionalLightSceneProxy::GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 InCascadeIndex, FWholeSceneProjectedShadowInitializer& OutInitializer) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);

	const FSphere Bounds = GetShadowSplitBounds(View, InCascadeIndex, &OutInitializer.CascadeSettings);
	const float R = Bounds.Radius;

	OutInitializer.PreShadowTranslation = -Bounds.Center;
	OutInitializer.WorldToLight = Dir->GetWorldRotation().Quaternion().ToFMatrix().GetTransposed();
	OutInitializer.Scales = FVector2(1.0f / R, 1.0f / R);
	OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(R), R);
	OutInitializer.WAxis = FVector4(0, 0, 0, 1);
	OutInitializer.MinLightW = -R * 2;
	OutInitializer.MaxDistanceToCastInLightW = R;

	return true;
}

FSphere FDirectionalLightSceneProxy::GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const
{
	const float SplitNear = GetSplitDistance(View, InCascadeIndex);
	const float SplitFar = GetSplitDistance(View, InCascadeIndex + 1);

	const FQuat Q = View.ViewRotation.Quaternion();
	const FVector Forward = Q.GetForwardVector();
	const FVector Right = Q.GetRightVector();
	const FVector Up = Q.GetUpVector();

	const float TanY = tanf(FMath::DegreesToRadians(View.FieldOfView) * 0.5f);
	const float TanX = TanY * View.AspectRatio;

	FVector Corners[8];
	const float Dist[2] = { SplitNear, SplitFar };
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector C = View.ViewLocation + Forward * Dist[i]; // 면의 중심
		const FVector R = Right * TanX * Dist[i];
		const FVector U = Up * TanY * Dist[i];
		Corners[i * 4 + 0] = C - R - U;
		Corners[i * 4 + 1] = C + R - U;
		Corners[i * 4 + 2] = C - R + U;
		Corners[i * 4 + 3] = C + R + U;
	}

	FVector Center = FVector::ZeroVector;
	for (const FVector& P : Corners) Center += P;
	Center = Center / 8.0f;

	float Radius = 0.0f;
	for (const FVector& P : Corners)
		Radius = std::max(Radius, (P - Center).Length());

	if (OutCascadeSettings)
	{
		OutCascadeSettings->SplitNear = SplitNear;
		OutCascadeSettings->SplitFar = SplitFar;
	}

	return FSphere(Center, Radius);
}

float FDirectionalLightSceneProxy::GetSplitDistance(const FSceneView& View, uint32 SplitIndex) const
{
	const float ShadowNear = View.NearClip;
	return ShadowNear + ComputeAccumulatedScale(CascadeDistributionExponent,
		static_cast<int32>(SplitIndex), static_cast<int32>(DynamicShadowCascades)) * (DynamicShadowDistanceMovableLight - ShadowNear);
}

void FPointLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* PointLightComponent = static_cast<const UPointLightComponent*>(LightComponent);
	const FLinearColor Color(PointLightComponent->GetLightColor());

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Point);
	Out.Position = PointLightComponent->GetWorldLocation();
	Out.Radius = PointLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = PointLightComponent->GetIntensity();
	Out.FalloffExponent = PointLightComponent->GetFalloff();
}

bool FPointLightSceneProxy::GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
{
	const UPointLightComponent* Point = Cast<const UPointLightComponent>(LightComponent);

	const FVector LightPos = Point->GetWorldLocation();
	const float Radius = Point->GetAttenuationRadius();

	const FRotator Faces[6] =
	{
		FRotator{ 0, 0, 0},
		FRotator{ 0, 180, 0},
		FRotator{ 0, 90, 0},
		FRotator{ 0, -90, 0},
		FRotator{ 90, 0, 0},
		FRotator{ -90, 0, 0},
	};

	for (const FRotator& Face : Faces)
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = OutInitializers.AddDefaulted_GetRef();
		OutInitializer.PreShadowTranslation = -LightPos;
		OutInitializer.WorldToLight = Face.Quaternion().ToFMatrix().GetTransposed();
		OutInitializer.Scales = FVector2(1.0f, 1.0f);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(Radius), Radius);
		OutInitializer.WAxis = FVector4(0, 0, 1, 0);
		OutInitializer.MinLightW = Radius * 0.02f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.bOnePassPointLightShadow = false;
	}

	return true;
}

void FSpotLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* SpotLightComponent = static_cast<const USpotLightComponent*>(LightComponent);
	const FLinearColor Color(SpotLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = SpotLightComponent->GetWorldRotation();

	const float Outer = FMath::Clamp(SpotLightComponent->GetOuterConeAngle(), 0.0f, 89.0f);
	const float Inner = FMath::Clamp(SpotLightComponent->GetInnerConeAngle(), 0.0f, Outer);

	Out = FLightRenderParameters{};
	Out.Position = SpotLightComponent->GetWorldLocation();
	Out.Radius = SpotLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = SpotLightComponent->GetIntensity();
	Out.FalloffExponent = SpotLightComponent->GetFalloff();

	Out.Type = static_cast<uint32>(ELightType::Spot);
	Out.Direction = Transform.GetForward();
	Out.InnerConeCos = std::cos(FMath::DegreesToRadians(Inner));
	Out.OuterConeCos = std::cos(FMath::DegreesToRadians(Outer));
}

bool FSpotLightSceneProxy::GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
{
	const USpotLightComponent* Spot = Cast<const USpotLightComponent>(LightComponent);

	const FVector LightPos = Spot->GetWorldLocation();
	const float Radius = Spot->GetAttenuationRadius();
	const float Outer = FMath::Clamp(Spot->GetOuterConeAngle(), 0.0f, 89.0f);
	const float InvTan = 1.0f / tanf(FMath::DegreesToRadians(Outer));

	FWholeSceneProjectedShadowInitializer& OutInitializer = OutInitializers.AddDefaulted_GetRef();
	OutInitializer.PreShadowTranslation = -LightPos;
	OutInitializer.WorldToLight = Spot->GetWorldRotation().Quaternion().ToFMatrix().GetTransposed();
	OutInitializer.Scales = FVector2(InvTan, InvTan);
	OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(Radius), Radius);
	OutInitializer.WAxis = FVector4(0, 0, 1, 0);
	OutInitializer.MinLightW = Radius * 0.02f;
	OutInitializer.MaxDistanceToCastInLightW = Radius;

	return true;
}

void FRectLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* RectLightComponent = static_cast<const URectLightComponent*>(LightComponent);
	const FLinearColor Color(RectLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = RectLightComponent->GetWorldRotation();

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Rect);
	Out.Position = RectLightComponent->GetWorldLocation();
	Out.Radius = RectLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = RectLightComponent->GetIntensity();
	Out.FalloffExponent = 1.0f;
	Out.Direction = Transform.GetForward();
	Out.Right = Transform.GetRight();
	Out.HalfWidth = RectLightComponent->GetWidth() * 0.5f;
	Out.HalfHeight = RectLightComponent->GetHeight() * 0.5f;

	const float AngleRad = FMath::DegreesToRadians(FMath::Clamp(RectLightComponent->GetBarnDoorAngle(), 0.0f, 90.0f));
	Out.BarnDoorCos = std::cos(AngleRad);
	Out.BarnDoorSin = std::sin(AngleRad);
	Out.BarnDoorLength = std::max(RectLightComponent->GetBarnDoorLength(), 0.0f);
}
