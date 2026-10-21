#pragma once

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

class AStaticMeshActor :public AActor
{
	DECLARE_CLASS(AStaticMeshActor, AActor)
public:
	AStaticMeshActor();
	virtual ~AStaticMeshActor() = default;

	virtual void BeginPlay() override;

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	void SetPrimitiveType(EPrimitiveType Type);

private:
	UStaticMeshComponent* StaticMeshComponent;
};

