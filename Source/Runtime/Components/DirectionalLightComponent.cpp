#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Engine/LightSceneProxy.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{

}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
    return new FDirectionalLightSceneProxy(this);
}

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "DynamicShadowDistanceMovableLight" ||
		Property.Name == "DynamicShadowCascades" ||
		Property.Name == "CascadeDistributionExponent"||
		Property.Name == "CascadeTransitionFraction"||
		Property.Name == "ShadowDistanceFadeoutFraction" ||
		Property.Name == "LightSourceAngle"
		)
		MarkRenderStateDirty();
}

void UDirectionalLightComponent::SetDynamicShadowDistanceMovableLight(float NewValue)
{
	if (DynamicShadowDistanceMovableLight != NewValue)
	{
		DynamicShadowDistanceMovableLight = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetDynamicShadowCascades(uint32 NewValue)
{
	if (DynamicShadowCascades != NewValue)
	{
		DynamicShadowCascades = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeDistributionExponent(float NewValue)
{
	if (CascadeDistributionExponent != NewValue)
	{
		CascadeDistributionExponent = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeTransitionFraction(float NewValue)
{
	if (CascadeTransitionFraction != NewValue)
	{
		CascadeTransitionFraction = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetShadowDistanceFadeoutFraction(float NewValue)
{
	if (ShadowDistanceFadeoutFraction != NewValue)
	{
		ShadowDistanceFadeoutFraction = NewValue;
		MarkRenderStateDirty();
	}
}
