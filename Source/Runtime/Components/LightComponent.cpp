#include "EnginePCH.h"
#include "LightComponent.h"

#include "Engine/Scene.h"
#include "Engine/World.h"

ULightComponent::ULightComponent() {}
ULightComponent::~ULightComponent() {}
void ULightComponent::DrawDebug(FLineBatcher* LineBatcher) const {}

void ULightComponent::SetShadowBias(float NewValue)
{
	if (ShadowBias != NewValue)
	{
		ShadowBias = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetShadowSlopeBias(float NewValue)
{
	if (ShadowSlopeBias != NewValue)
	{
		ShadowSlopeBias = NewValue;
		MarkRenderStateDirty();
	}
}

bool ULightComponent::CastsDynamicShadow() const
{
	return CastShadows && CastDynamicShadows;
}

// Proxy가 그림자 값을 복사해 두므로, 값이 바뀌면 Proxy를 새로 만든다.
void ULightComponent::MarkRenderStateDirty()
{
	if (!IsRegistered() || !IsRenderStateCreated() || !GetWorld()) return;
	DestroyRenderState();
	CreateRenderState();
}

void ULightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "ShadowResolutionScale" ||
		Property.Name == "ShadowBias" ||
		Property.Name == "ShadowSlopeBias" ||
		Property.Name == "ShadowSharpen")
		MarkRenderStateDirty();
}


void ULightComponent::CreateRenderState()
{
	Super::CreateRenderState();
	GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveLight(this);
	Super::DestroyRenderState();
}
