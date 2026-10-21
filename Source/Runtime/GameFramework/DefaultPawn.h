#pragma once

#include "GameFramework/Pawn.h"
#include <array>

class UCameraComponent;
class UStaticMeshComponent;
class UStaticMesh;
class AProjectileActor;

class ADefaultPawn : public APawn
{
	DECLARE_CLASS(ADefaultPawn, APawn)
	REFLECT_START(ClassName)
		PROPERTY(ProjectileMesh)
	REFLECT_END()
public:
	ADefaultPawn();
	void BeginPlay() override;
	void AddMovementInput(const FVector& WorldDirection, float ScaleValue) override;
	void AddControllerYawInput(float Value) override;
	void AddControllerPitchInput(float Value) override;
	void FireProjectile();
	static constexpr int32 ProjectilePoolSize = 16;
	const std::array<AProjectileActor*, ProjectilePoolSize>& GetProjectiles() const { return Projectiles; }
	void SetProjectileMesh(UStaticMesh* Mesh) { ProjectileMesh = Mesh; }
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	UStaticMesh* GetProjectileMesh() const { return ProjectileMesh; }
private:
	UCameraComponent* CameraComponent = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	float MoveSpeed = 20.0f;
	UStaticMesh* ProjectileMesh = nullptr;
	std::array<AProjectileActor*, ProjectilePoolSize> Projectiles{};
	int32 NextProjectileIndex = 0;
};
