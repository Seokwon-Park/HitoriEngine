#pragma once

#include "GameFramework/Actor.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;

// Reusable PIE projectile launched by the default pawn.
class AProjectileActor : public AActor
{
    DECLARE_CLASS(AProjectileActor, AActor)
    REFLECT_START(ClassName)
    REFLECT_END()
public:
    AProjectileActor();
    void Tick(float DeltaTime) override;
    void Launch(const FVector& Position, const FVector& Direction);
    void ReturnToPool();
    bool IsInFlight() const { return bInFlight; }
    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }
private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UProjectileMovementComponent* ProjectileMovementComponent = nullptr;
    bool bInFlight = false;
    float RemainingLife = 0.0f;
    float LifeTime = 5.0f;
};
