#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSubUVComponent.h"

// Todo: subuv
class AParticleActor : public AActor
{
	DECLARE_CLASS(AParticleActor, AActor)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	AParticleActor();
	//UParticleSubUVComponent* GetParticleComponent() const;

private:
	UParticleSubUVComponent* ParticleComponent;
};
