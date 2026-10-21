#pragma once

#include "LocalLightComponent.h"

class URectLightComponent :public ULocalLightComponent
{
	DECLARE_CLASS(URectLightComponent, ULocalLightComponent)
	REFLECT_START(URectLightComponent)
		PROPERTY(HalfWidth)
		PROPERTY(HalfHeight)
		PROPERTY(BarnDoorAngle)
		PROPERTY(BarnDoorLength)
	REFLECT_END()
public:
	URectLightComponent();
	virtual ~URectLightComponent() override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;

	ELightType GetLightType() const { return ELightType::Rect; }
	void DrawDebug(FLineBatcher* LineBatcher) const;

	// Getter
	float GetWidth() const { return HalfWidth * 2.0f; }
	float GetHeight() const { return HalfHeight * 2.0f; }

	void SetBarnDoorAngle(float InAngle) { BarnDoorAngle = std::max(0.0f, InAngle); }
	float GetBarnDoorAngle() const { return BarnDoorAngle; }

	void SetBarnDoorLength(float InLength) { BarnDoorLength = std::clamp(InLength, 0.0f, 90.0f); }
	float GetBarnDoorLength() const { return BarnDoorLength; }

private:
	float HalfWidth = 3.0f;
	float HalfHeight = 3.0f;
	float BarnDoorAngle = 88.0f;	// 도 단위
	float BarnDoorLength = 2.0f;	
};