#pragma once

#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "Components/ExponentialHeightFogComponent.h"


class AExponentialHeightFogActor final : public AActor
{
	DECLARE_CLASS(AExponentialHeightFogActor, AActor)




public:
	AExponentialHeightFogActor();
	virtual ~AExponentialHeightFogActor() override = default;

	AExponentialHeightFogActor(const AExponentialHeightFogActor&) = delete;
	AExponentialHeightFogActor operator=(const AExponentialHeightFogActor&) = delete;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UExponentialHeightFogComponent* GetExponentialHeightFogComponent() const { return ExponentialHeightFogComponent; }

private:
	UExponentialHeightFogComponent* ExponentialHeightFogComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};