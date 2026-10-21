#pragma once
#include "SceneComponent.h"
#include "Math/Vector4.h"

class UExponentialHeightFogComponent final : public USceneComponent
{
	DECLARE_CLASS(UExponentialHeightFogComponent, USceneComponent)
	REFLECT_START(UExponentialHeightFogComponent)
		PROPERTY(FogDensity)
		PROPERTY(FogHeightFalloff)
		PROPERTY_TYPE(FogColor, Color)
		PROPERTY(StartDistance)
		PROPERTY(FogMaxOpacity)
		PROPERTY(FogCutoffDistance)
	REFLECT_END()

public:
	UExponentialHeightFogComponent() = default;
	~UExponentialHeightFogComponent() override = default;

	float GetFogDensity() const { return FogDensity; }
	void SetFogDensity(float InDensity) { FogDensity = InDensity; }

	float GetFogHeightFalloff() const { return FogHeightFalloff; }
	void SetFogHeightFalloff(float InFalloff) { FogHeightFalloff = InFalloff; }

	const FVector4& GetFogColor() const { return FogColor; }
	void SetFogColor(const FVector4& InColor) { FogColor = InColor; }

	float GetStartDistance() const { return StartDistance; }
	void SetStartDistance(float InDistance) { StartDistance = InDistance; }

	float GetFogMaxOpacity() const { return FogMaxOpacity; }
	void SetFogMaxOpacity(float InOpacity) { FogMaxOpacity = InOpacity; }

	float GetFogCutoffDistance() const { return FogCutoffDistance; }
	void SetFogCutoffDistance(float InDistance) { FogCutoffDistance = InDistance; }

	float GetFogHeight() const { return GetWorldLocation().Z; }

private:
	float FogDensity = 0.02f;
	float FogHeightFalloff = 0.02f;
	FVector4 FogColor = FVector4(0.5f, 0.6f, 0.7f, 1.0f);
	float StartDistance = 0.0f;
	float FogMaxOpacity = 1.0f;
	float FogCutoffDistance = 0.0f;
};