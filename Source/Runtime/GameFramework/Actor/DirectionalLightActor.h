#pragma once

#include "GameFramework/Actor/LightActorBase.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"

// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 스포트라이트 원뿔이 라인으로 그려진다.
class ADirectionalLightActor : public ALightActorBase
{
	DECLARE_CLASS(ADirectionalLightActor, ALightActorBase)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ADirectionalLightActor();
	virtual ~ADirectionalLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return DirectionalLightComponent; }
    UDirectionalLightComponent* GetPointLightComponent() const { return DirectionalLightComponent; }

private:
    UDirectionalLightComponent* DirectionalLightComponent = nullptr;
    UStaticMeshComponent* ArrowComponent = nullptr;

};
