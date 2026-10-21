#pragma once

#include "LightComponent.h"

class UDirectionalLightComponent :public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(DynamicShadowDistanceMovableLight)
		PROPERTY(DynamicShadowCascades)
		PROPERTY(CascadeDistributionExponent)
		PROPERTY(CascadeTransitionFraction)
		PROPERTY(ShadowDistanceFadeoutFraction)

		PROPERTY(LightSourceAngle)
	REFLECT_END()
public:
	UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

	virtual class FLightSceneProxy* CreateSceneProxy() const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	ELightType GetLightType() const override { return ELightType::Directional; }


	void SetDynamicShadowDistanceMovableLight(float NewValue);
	void SetDynamicShadowCascades(uint32 NewValue);
	void SetCascadeDistributionExponent(float NewValue);
	void SetCascadeTransitionFraction(float NewValue);
	void SetShadowDistanceFadeoutFraction(float NewValue);

	float DynamicShadowDistanceMovableLight = 100.0f;
	uint32 DynamicShadowCascades = 4;
	float CascadeDistributionExponent = 1.5f;
	float CascadeTransitionFraction = 0.1f;
	float ShadowDistanceFadeoutFraction = 0.1f;

	//PCSS
	float LightSourceAngle = 5.0f;

private:

};