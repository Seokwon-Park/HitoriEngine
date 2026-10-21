#pragma once

#include "LightComponent.h"

class ULocalLightComponent :public ULightComponent
{
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)
	REFLECT_START(ULocalLightComponent)
		PROPERTY(AttenuationRadius)
	REFLECT_END()
public:
	ULocalLightComponent();
	virtual ~ULocalLightComponent() override;

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InAttenuationRadius) { AttenuationRadius = std::max(InAttenuationRadius, 0.01f); }

private:
	float AttenuationRadius = 5.0f;
};