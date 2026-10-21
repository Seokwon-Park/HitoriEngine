#pragma once

#include "GameFramework/Actor/LightActorBase.h"
#include "Components/PointLightComponent.h"

class APointLightActor : public ALightActorBase
{
	DECLARE_CLASS(APointLightActor, ALightActorBase)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return PointLightComponent; }
    UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
    UPointLightComponent* PointLightComponent = nullptr;

};
