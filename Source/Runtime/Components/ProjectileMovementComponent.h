#pragma once
#include "MovementComponent.h"

class UProjectileMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

		REFLECT_START(ClassName)
			PROPERTY(InintialSpeed)
			PROPERTY(MaxSpeed)
			PROPERTY(ProjectileGravityScale)
			PROPERTY(bRotationFollowsVelocity)
			PROPERTY(bShouldBounce)
			PROPERTY(Bounciness)
			PROPERTY(Friction)
			PROPERTY(bSimulationEnabled)
			PROPERTY(bSweepCollision)
			PROPERTY(bIsHomingProjectile)
			PROPERTY(HomingAccelerationMagnitude)
			PROPERTY(Velocity)
			PROPERTY(Gravity)
			PROPERTY(bConstrainToPlane)
		REFLECT_END()

public:
	UProjectileMovementComponent();

	// Projectile
	float InintialSpeed = 0.0f;
	float MaxSpeed = 0.0f;
	float ProjectileGravityScale = 1.0f;

	bool bRotationFollowsVelocity;
	bool bInitialVelocityInLocalSpace;

	float PreviousHitTime;
	FVector PreviousHitNormal;

	bool bIsSliding = false;

	// Bounce
	bool bShouldBounce;
	float Bounciness;
	float Friction;	

	// Simulation
	bool bSimulationEnabled;
	bool bSweepCollision;
	float MaxSimulationTimeStep = 0.0166f; // Min : 0.0166, Max : 0.50 in Unreal 
	int32 MaxSimulationIterations = 5; // Min : 1, Max : 25 in Unreal

	// Homing
	bool bIsHomingProjectile;
	float HomingAccelerationMagnitude;
	USceneComponent* HomingTargetComponent = nullptr;

	// Interpolation

public:
	// Interface
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime) override;

	// Projectile
	virtual void SetVelocityInLocalSpace(FVector NewVelocity);

	FVector LimitVelocity(FVector NewVelocity) const;
	virtual FVector ComputeVelocity(FVector InitialVelocity, float DeltaTime) const;
	virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;
	virtual FVector ComputAcceleration(const FVector& InVelocity, float DeltaTime) const;
	virtual FVector ComputeHomingAcceleration(const FVector& InVelocity, float DeltaTime) const;
	virtual FVector ComputeBounceResult(const FHitResult& Hit, float Timeslice, const FVector& MoveDelta);

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector) override;
	virtual bool HandleDeflection(FHitResult& Hit, const FVector& OldVelocity, float& SubTickTimeRemaining);	
	virtual bool HandleSliding(FHitResult& Hit, float& SubTickTimeRemaining);

	void AddForce(FVector Force);
	FVector GetPendingForce() const;
	void ClearPendingForce(bool bClearImmediateForce = false);

	float GetSimulationTimeStep(float RemainingTime, int32 Iterations) const;
	bool HasStoppedSimulation() const { return !bSimulationEnabled || UpdatedComponent == nullptr; }

private:
	FVector PendingForce;
	FVector PendingForceThisUpdate;

protected:
	static const float MIN_TICK_TIME;
};
