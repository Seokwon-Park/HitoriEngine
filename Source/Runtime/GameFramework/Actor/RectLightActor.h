#pragma once

#include "Components/RectLightComponent.h"
#include "GameFramework/Actor/LightActorBase.h"
#include "Components/StaticMeshComponent.h"

class ARectLightActor : public ALightActorBase
{
	DECLARE_CLASS(ARectLightActor, ALightActorBase)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ARectLightActor();
	virtual ~ARectLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return RectLightComponent; }
    URectLightComponent* GetRectLightComponent() const { return RectLightComponent; }

private:
    URectLightComponent* RectLightComponent = nullptr;
	UStaticMeshComponent* ArrowComponent = nullptr;

};
