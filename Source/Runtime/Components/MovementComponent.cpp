#include "EnginePCH.h"
#include "MovementComponent.h"
#include "GameFramework/Actor.h"

void UMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (AActor* MyActor = GetOwner())
	{
		if (USceneComponent* NewUpdatedCompoent = MyActor->GetRootComponent())
		{
			SetUpdatedComponent(NewUpdatedCompoent);
		}
	}

	PrimaryComponentTick.bCanEverTick = true;
}

void UMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}
  
void UMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewComponent)
{
	if (NewComponent == nullptr)
	{
		return;
	}

	UpdatedComponent = NewComponent;
}

void UMovementComponent::UpdateComponentVelocity()
{
	if (UpdatedComponent)
	{
		UpdatedComponent->ComponentVelocity = Velocity;
	}
}

void UMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{

}

FVector UMovementComponent::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& HIt) const
{
	if (!bConstrainToPlane)
	{
		return FVector::VectorPlaneProject(Delta, Normal) * Time;		
	}
	else
	{
		const FVector ProjectedNormal = ConstrainDirectionToPlane(Normal);
		return FVector::VectorPlaneProject(Delta, ProjectedNormal) * Time;
	}
}

bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	if (UpdatedComponent)
	{
		return UpdatedComponent->MoveComponent(Delta, NewRotation, bSweep, Hit);		
	}

	return false;
}

void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

FVector UMovementComponent::ConstrainDirectionToPlane(FVector Direction) const
{
	if (bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, PlaneConstraintNormal);
	}

	return Direction;
}
