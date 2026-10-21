#include "EnginePCH.h"
#include "GameFramework/DefaultPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor/ProjectileActor.h"

ADefaultPawn::ADefaultPawn()
{
	SetActorEnableCollision(false);
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	SetRootComponent(StaticMeshComponent);
	StaticMeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Models/Hitori/Hitori.obj"));
	ProjectileMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere");
	CameraComponent->SetupAttachment(StaticMeshComponent);
	CameraComponent->SetRelativeLocation({-5.0f, 0.0f, 8.0f});
	CameraComponent->SetRelativeRotation({45.0f, 0.0f, 0.0f});
	CameraComponent->SetExternalInputManaged(true);
}

void ADefaultPawn::BeginPlay()
{
	Super::BeginPlay();
	UWorld* PawnWorld = GetWorld();
	if (!PawnWorld || PawnWorld->GetWorldType() != EWorldType::PIE) return;
	for (AProjectileActor*& Projectile : Projectiles)
		if (!Projectile) Projectile = PawnWorld->SpawnActor<AProjectileActor>();
}

void ADefaultPawn::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	if (GetRootComponent())
		GetRootComponent()->SetRelativeLocation(GetActorLocation() + WorldDirection * (ScaleValue * MoveSpeed));
}

void ADefaultPawn::AddControllerYawInput(float Value)
{
	if (!GetRootComponent()) return;
	FRotator Rotation = GetActorRotation();
	Rotation.Yaw += Value;
	GetRootComponent()->SetRelativeRotation(Rotation);
}

void ADefaultPawn::AddControllerPitchInput(float Value)
{
	if (!GetRootComponent()) return;
	FRotator Rotation = GetActorRotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + Value, -89.0f, 89.0f);
	GetRootComponent()->SetRelativeRotation(Rotation);
}

void ADefaultPawn::FireProjectile()
{
	if (!GetWorld() || GetWorld()->GetWorldType() != EWorldType::PIE || !CameraComponent) return;
	int32 SelectedIndex = -1;
	for (int32 Offset = 0; Offset < ProjectilePoolSize; ++Offset)
	{
		const int32 Index = (NextProjectileIndex + Offset) % ProjectilePoolSize;
		if (Projectiles[Index] && !Projectiles[Index]->IsInFlight()) { SelectedIndex = Index; break; }
	}
	if (SelectedIndex < 0) SelectedIndex = NextProjectileIndex;
	AProjectileActor* Projectile = Projectiles[SelectedIndex];
	if (!Projectile)
	{
		HTR_LOG(Warning, "FireProjectile: projectile pool slot {} is empty", SelectedIndex);
		return;
	}
	const FVector Forward = CameraComponent->GetWorldRotation().Quaternion().GetForwardVector().Normalized();
	const FVector Position = CameraComponent->GetWorldLocation() + Forward * 2.0f;
	// 투사체 메쉬를 고르지 않았으면 기본 구체를 쓴다.
	Projectile->GetStaticMeshComponent()->SetStaticMesh(ProjectileMesh ? ProjectileMesh : UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"));
	Projectile->Launch(Position, Forward);
	if (Projectile->IsInFlight())
	{
		HTR_LOG(Info, "FireProjectile: launched slot {}", SelectedIndex);
		NextProjectileIndex = (SelectedIndex + 1) % ProjectilePoolSize;
	}
	else
		HTR_LOG(Warning, "FireProjectile: Launch did not start slot {}", SelectedIndex);
}
