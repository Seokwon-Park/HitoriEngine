#include "EnginePCH.h"
#include "RotatingMovementComponent.h"

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (UpdatedComponent == nullptr)
	{
		return;
	}

	const FQuat OldRotation = UpdatedComponent->GetRelativeRotationQuat();
	const FQuat DeltaRotation = (RotationRate * DeltaTime).Quaternion();
	const FQuat NewRotation = bRotaionInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);

	// Compute New Location	
	FVector DeltaLocation = FVector::ZeroVector;
	if (PivotTranslation != FVector::ZeroVector)
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = (OldPivot - NewPivot);
	}

	const bool bEnableCollision = false;
	MoveUpdatedComponent(DeltaLocation, NewRotation.ToFRotator(), bEnableCollision, nullptr);
}
