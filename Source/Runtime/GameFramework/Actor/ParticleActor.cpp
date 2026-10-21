#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>("UParticleSubUVComponent");
	ParticleComponent->SetupAttachment(GetRootComponent());

	ParticleComponent->SetSubUVSize(8, 8);
	ParticleComponent->SetFrameRate(12.0f);
}

//UParticleSubUVComponent* AParticleActor::GetParticleComponent() const
//{
//	return static_cast<UParticleSubUVComponent*>(RootComponent);
//}


