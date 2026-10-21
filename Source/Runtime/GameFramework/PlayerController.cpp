#include "EnginePCH.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include <cstring>

APlayerController::APlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ProcessPlayerInput(DeltaTime);
}

void APlayerController::ProcessPlayerInput(float DeltaTime)
{
	if (!PossessedPawn) return;
	const float Forward = static_cast<float>(bKeyDown[static_cast<int32>(EKeyCode::W)]) -
		static_cast<float>(bKeyDown[static_cast<int32>(EKeyCode::S)]);
	const float Right = static_cast<float>(bKeyDown[static_cast<int32>(EKeyCode::D)]) -
		static_cast<float>(bKeyDown[static_cast<int32>(EKeyCode::A)]);
	if (bKeyDown[static_cast<int32>(EKeyCode::RButton)])
	{
		PossessedPawn->AddControllerYawInput(MouseAxisX * LookSensitivity);
		PossessedPawn->AddControllerPitchInput(MouseAxisY * LookSensitivity);
	}
	MouseAxisX = MouseAxisY = 0.0f;
	const FQuat Rotation = PossessedPawn->GetActorRotation().Quaternion();
	if (Forward != 0.0f) PossessedPawn->AddMovementInput(Rotation.GetForwardVector(), Forward * DeltaTime);
	if (Right != 0.0f) PossessedPawn->AddMovementInput(Rotation.GetRightVector(), Right * DeltaTime);
}

void APlayerController::GetPlayerViewCamera(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!PossessedPawn) return;
	OutLocation = PossessedPawn->GetActorLocation();
	OutRotation = PossessedPawn->GetActorRotation();
}

void APlayerController::Possess(APawn* InPawn)
{
	if (PossessedPawn == InPawn) return;
	UnPossess();
	PossessedPawn = InPawn;
	if (PossessedPawn) PossessedPawn->PossessedBy(this);
}

void APlayerController::UnPossess()
{
	if (PossessedPawn) PossessedPawn->UnPossessed();
	PossessedPawn = nullptr;
}

void APlayerController::InputKey(int32 Key, bool bDown)
{
	if (Key < 0 || Key >= 256) return;
	const bool bPressed = bDown && !bKeyDown[Key];
	bKeyDown[Key] = bDown;
	if (bPressed && Key == static_cast<int32>(EKeyCode::LButton))
		if (ADefaultPawn* Pawn = Cast<ADefaultPawn>(PossessedPawn)) Pawn->FireProjectile();
}

void APlayerController::InputAxis(EGameInputAxis Axis, float Delta)
{
	if (Axis == EGameInputAxis::MouseX) MouseAxisX += Delta;
	else if (Axis == EGameInputAxis::MouseY) MouseAxisY += Delta;
}

void APlayerController::ClearInputState()
{
	std::memset(bKeyDown, 0, sizeof(bKeyDown));
	MouseAxisX = MouseAxisY = 0.0f;
}
