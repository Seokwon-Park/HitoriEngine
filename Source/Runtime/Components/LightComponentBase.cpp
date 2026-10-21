#include "EnginePCH.h"
#include "LightComponentBase.h"

ULightComponentBase::ULightComponentBase()
	: Intensity(1.0f), LightColor(255.0f, 255.0f, 255.0f, 255.0f)
{
}

ULightComponentBase::~ULightComponentBase() {}

void ULightComponentBase::SetCastShadows(bool bNewValue)
{
	if (CastShadows != bNewValue)
	{
		CastShadows = bNewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponentBase::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "CastShadows" ||
		Property.Name == "CastDynamicShadows")
		MarkRenderStateDirty();
}
