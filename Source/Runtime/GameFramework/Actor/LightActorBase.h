#pragma once

#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "Components/LightComponent.h"

class ALightActorBase : public AActor
{
    DECLARE_CLASS(ALightActorBase, AActor)

    REFLECT_START(ALightActorBase)
    REFLECT_END()

public:
	ALightActorBase();
	virtual ~ALightActorBase() override = default;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	virtual ULightComponent* GetLightComponent() const = 0;

protected:
	void SetupLightRoot(ULightComponent* LightComponent, const FString& IconPath);

private:
	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;
};