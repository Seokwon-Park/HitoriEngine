#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"
#include <cmath>

const float UProjectileMovementComponent::MIN_TICK_TIME = 1e-6f;

UProjectileMovementComponent::UProjectileMovementComponent()
{
	// Default
	bInitialVelocityInLocalSpace = true;
	bSimulationEnabled = true;
	bSweepCollision = true;
	bRotationFollowsVelocity = false;
	bShouldBounce = true;
	bIsHomingProjectile = false;
	PreviousHitTime = 1.0f;

	Velocity = FVector(1.0f, 0.0f, 0.0f);

	ProjectileGravityScale = 1.0f;

	Bounciness = 0.6f;
	Friction = 0.2f;

	HomingAccelerationMagnitude = 0.0f;
}

void UProjectileMovementComponent::InitializeComponent()
{	
	Super::InitializeComponent();

	if (Velocity.SizeSquared() > 0.0f)
	{
		if (InintialSpeed > 0.0f)
		{
			Velocity = Velocity.Normalized() * InintialSpeed;
		}

		if (bInitialVelocityInLocalSpace)
		{
			SetVelocityInLocalSpace(Velocity);
		}

		if (bRotationFollowsVelocity)
		{

		}

		UpdateComponentVelocity();
	}

}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{		
	PendingForceThisUpdate = PendingForce;
	ClearPendingForce();

	Super::TickComponent(DeltaTime);

	if (!bSimulationEnabled || DeltaTime <= 0.0f || UpdatedComponent == nullptr)
	{
		return;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (ActorOwner == nullptr)
	{
		return;
	}

	float RemainingTime = DeltaTime;
	int32 Iterations = 0;
	FHitResult Hit(1.0f);

	while (bSimulationEnabled && RemainingTime >= MIN_TICK_TIME && Iterations < MaxSimulationIterations)
	{
		Iterations++;

		const float TimeTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= TimeTick;

		// Initial move state
		Hit.Time = 1.0f;
		const FVector OldVelocity = Velocity;
		const FVector MoveDelta = ComputeMoveDelta(OldVelocity, TimeTick);
		FRotator NewRotation = UpdatedComponent->GetWorldRotation();
		if (bRotationFollowsVelocity && OldVelocity.SizeSquared() > 0.0f)
		{
			const float HorizontalSpeed = std::sqrt(OldVelocity.X * OldVelocity.X + OldVelocity.Y * OldVelocity.Y);
			NewRotation = FRotator(
				FMath::RadiansToDegrees(std::atan2(-OldVelocity.Z, HorizontalSpeed)),
				FMath::RadiansToDegrees(std::atan2(OldVelocity.Y, OldVelocity.X)), 0.0f);
		}

		// Move the component
		MoveUpdatedComponent(MoveDelta, NewRotation, bSweepCollision, &Hit);

		// Handle hit result after movement
		if (!Hit.bBlockingHit)
		{
			PreviousHitTime = 1.0f;
			bIsSliding = false;

			if (Velocity == OldVelocity)
			{
				Velocity = ComputeVelocity(Velocity, TimeTick);
			}
		}
		else
		{
			if (Velocity == OldVelocity)
			{
				Velocity = (Hit.Time > KINDA_SMALL_NUMBER ? ComputeVelocity(OldVelocity, TimeTick * Hit.Time) : OldVelocity);
			}

			float SubTickTimeRemaining = TimeTick * (1.0f - Hit.Time);
			HandleImpact(Hit, SubTickTimeRemaining, MoveDelta);
			if (!bSimulationEnabled) break;
			PreviousHitTime = Hit.Time;
			PreviousHitNormal = ConstrainDirectionToPlane(Hit.Normal);

			if (SubTickTimeRemaining >= MIN_TICK_TIME && Hit.Time > KINDA_SMALL_NUMBER)
			{
				RemainingTime += SubTickTimeRemaining;
			}
		}
	}

	UpdateComponentVelocity();
}

void UProjectileMovementComponent::SetVelocityInLocalSpace(FVector NewVelocity)
{
	if (UpdatedComponent)
	{
		Velocity = UpdatedComponent->GetWorldMatrix().TransformVectorNoScale(NewVelocity);
	}
}

FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	if (MaxSpeed > 0.0f)
	{
		NewVelocity = NewVelocity.GetClampedToMaxSize(MaxSpeed);
	}

	return ConstrainDirectionToPlane(NewVelocity);
}

FVector UProjectileMovementComponent::ComputeVelocity(FVector InitialVelocity, float DeltaTime) const
{
	// v = v0 + a*t
	const FVector Acceleration = ComputAcceleration(InitialVelocity, DeltaTime);
	FVector NewVelocity = InitialVelocity + (Acceleration * DeltaTime);

	return LimitVelocity(NewVelocity);
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	// p = p0 + v0*t + 1/2*a*t^2

	const FVector NewVelocity = ComputeVelocity(InVelocity, DeltaTime);
	const FVector Delta = (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);

	return Delta;
}

FVector UProjectileMovementComponent::ComputAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector Acceleration(FVector::ZeroVector);	

	Acceleration.Z -= Gravity * ProjectileGravityScale;
	
	Acceleration += PendingForceThisUpdate;

	if (bIsHomingProjectile && HomingTargetComponent != nullptr)
	{
		Acceleration += ComputeHomingAcceleration(InVelocity, DeltaTime);
	}

	return Acceleration;
}

FVector UProjectileMovementComponent::ComputeHomingAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector HomingAcceleration = (HomingTargetComponent->GetWorldLocation() - UpdatedComponent->GetWorldLocation()).Normalized();
	return HomingAcceleration;
}

FVector UProjectileMovementComponent::ComputeBounceResult(const FHitResult& Hit, float Timeslice, const FVector& MoveDelta)
{
	FVector TempVelocity = Velocity;
	const FVector Normal = ConstrainDirectionToPlane(Hit.Normal);
	const float VDotNormal = (TempVelocity | Normal);

	// Opposed by normal or parallel
	if (VDotNormal <= 0.0f)
	{
		const FVector ProjectedNormal = Normal * -VDotNormal;

		TempVelocity += ProjectedNormal;

		// TODO : Set MinFrictionFraction
		//const float SacledFriction = (bIsSliding) ? FMath::Clamp(-VDotNormal / TempVelocity.Size(), MinFrictionFraction, 1.0f) * Friction : Friction;
		const float ScaledFriction = (bIsSliding && TempVelocity.Size() > KINDA_SMALL_NUMBER) ? FMath::Clamp(-VDotNormal / TempVelocity.Size(), 0.0f, 1.0f) * Friction : Friction;
		TempVelocity *= FMath::Clamp(1.0f - ScaledFriction, 0.0f, 1.0f);

		// TODO : Change std::max to FMath::Max
		TempVelocity += (ProjectedNormal * std::max(Bounciness, 0.0f));

		TempVelocity = LimitVelocity(TempVelocity);
	}

	return TempVelocity;
}

void UProjectileMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{	
	if (bShouldBounce)
	{
		const FVector OldVelocity = Velocity;
		Velocity = ComputeBounceResult(Hit, TimeSlice, MoveDelta);

		Velocity = LimitVelocity(Velocity);
		if (Velocity.SizeSquared() < 0.01f)
		{
			Velocity = FVector::ZeroVector;
			bSimulationEnabled = false;
		}
	}
	else
	{
		bSimulationEnabled = false;
		Velocity = FVector::ZeroVector;
	}
}

bool UProjectileMovementComponent::HandleDeflection(FHitResult& Hit, const FVector& OldVelocity, float& SubTickTimeRemaining)
{
	const FVector Normal = ConstrainDirectionToPlane(Hit.Normal);

	const float DotTolerance = 0.01f;
	bIsSliding = FVector::Coincident(PreviousHitNormal, Normal) || ((Velocity.Normalized() | Normal) <= DotTolerance);

	if (bIsSliding)
	{		
		if ((PreviousHitNormal | Normal) <= 0.0f) // Opposed by normal or parallel
		{
			// 90 or less, so cross product for direction
			FVector NewDir = (Normal ^ PreviousHitNormal);
			NewDir = NewDir.Normalized();
			Velocity = Velocity.ProjectOnToNormal(NewDir);
			if ((OldVelocity | Velocity) < 0.0f)
			{
				Velocity *= -1.0f;
			}
			Velocity = ConstrainDirectionToPlane(Velocity);
		}
		else
		{
			// move to new wall
			Velocity = ComputeSlideVector(Velocity, 1.0f, Normal, Hit);
		}
	}

	return true;
}

bool UProjectileMovementComponent::HandleSliding(FHitResult& Hit, float& SubTickTimeRemaining)
{
	FHitResult InitailHit;
	const FVector OldHitNormal = ConstrainDirectionToPlane(Hit.Normal);

	MoveUpdatedComponent(Velocity * SubTickTimeRemaining, UpdatedComponent->GetRelativeRotation(), bSweepCollision, &Hit);

	if (Hit.bBlockingHit)
	{
		const float TimeTick = SubTickTimeRemaining;
		SubTickTimeRemaining = TimeTick * (1.0f - Hit.Time);
	}
	else
	{
		const FVector PostTickVelocity = ComputeVelocity(Velocity, SubTickTimeRemaining);

		const FVector Force = (PostTickVelocity - Velocity);
		const float ForceDotN = (Force | OldHitNormal);
		if (ForceDotN < 0.0f)
		{
			const FVector ProjectedForce = FVector::VectorPlaneProject(Force, OldHitNormal);
			const FVector NewVelocity = Velocity + ProjectedForce;

			const FVector FrictionForce = -NewVelocity.GetSafeNormal() * (-ForceDotN * Friction < NewVelocity.Size() ? -ForceDotN * Friction : NewVelocity.Size()); // TODO : Change to FMath::Min
			Velocity = ConstrainDirectionToPlane(NewVelocity + FrictionForce);
		}
		else
		{
			Velocity = PostTickVelocity;
		}

		SubTickTimeRemaining = 0.0f;
	}

	return true;
}

void UProjectileMovementComponent::AddForce(FVector Force)
{
	PendingForce += Force;
}

FVector UProjectileMovementComponent::GetPendingForce() const
{
	return PendingForce;
}

void UProjectileMovementComponent::ClearPendingForce(bool bClearImmediateForce)
{
	PendingForce = FVector::ZeroVector;
}

float UProjectileMovementComponent::GetSimulationTimeStep(float RemainingTime, int32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep && Iterations < MaxSimulationIterations)
	{
		RemainingTime = std::min(MaxSimulationTimeStep, RemainingTime * 0.5f);
	}

	return std::max(MIN_TICK_TIME, RemainingTime);
}
