#pragma once

#include "GameFramework/Actor.h"
#include "Input/InputSystem.h"

class APawn;

class APlayerController : public AActor
{
	DECLARE_CLASS(APlayerController, AActor)
	REFLECT_START(ClassName)
	REFLECT_END()
public:
	APlayerController();
	void Tick(float DeltaTime) override;
	void ProcessPlayerInput(float DeltaTime);
	void GetPlayerViewCamera(FVector& OutLocation, FRotator& OutRotation) const;
	void Possess(APawn* InPawn);
	void UnPossess();
	APawn* GetPawn() const { return PossessedPawn; }
	void InputKey(int32 Key, bool bDown);
	void InputAxis(EGameInputAxis Axis, float Delta);
	void ClearInputState();
private:
	APawn* PossessedPawn = nullptr;
	bool bKeyDown[256]{};
	float MouseAxisX = 0.0f;
	float MouseAxisY = 0.0f;
	float LookSensitivity = 0.1f;
};
