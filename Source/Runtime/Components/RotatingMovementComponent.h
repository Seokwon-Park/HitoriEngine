#pragma once
#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

		REFLECT_START(ClassName)
			PROPERTY(RotationRate)
			PROPERTY(PivotTranslation)
			PROPERTY(bRotaionInLocalSpace)									
		REFLECT_END()

public:	
	FRotator RotationRate = FRotator(0,0,0);
	FVector PivotTranslation = FVector::ZeroVector; // Rotate Pivot or Orbit
	bool bRotaionInLocalSpace = true;

	virtual void TickComponent(float DeltaTime) override;
};