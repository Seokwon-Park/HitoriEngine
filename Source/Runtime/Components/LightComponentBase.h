#pragma once

#include "SceneComponent.h"
#include "Math/Color.h"

enum class ELightType { Directional, Point, Spot, Rect, Sky };

class ULightComponentBase : public USceneComponent
{
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	REFLECT_START(ULightComponentBase)
		PROPERTY(Intensity)
		PROPERTY(LightColor)
		PROPERTY(CastShadows)
		PROPERTY(CastDynamicShadows)
		REFLECT_END()
public:
	ULightComponentBase();
	virtual ~ULightComponentBase() override;

	// Getter
	float GetIntensity() const { return Intensity; }
	FColor GetLightColor() const { return LightColor; }

	// Setter
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	void SetLightColor(FColor InLightColor) { LightColor = InLightColor; }

	void SetCastShadows(bool bNewValue);

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	bool CastShadows = true;
	bool CastDynamicShadows = true;

	float Intensity = 1.0f;
	FColor LightColor = FColor(255, 255, 255, 255);
private:
};
