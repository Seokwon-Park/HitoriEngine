#pragma once

#include "GameFramework/Actor.h"

class APlayerController;

class APawn : public AActor
{
	DECLARE_CLASS(APawn, AActor)
	REFLECT_START(ClassName)
	REFLECT_END()
public:
	APawn();
	virtual void AddMovementInput(const FVector& WorldDirection, float ScaleValue) {}
	virtual void AddControllerYawInput(float Value) {}
	virtual void AddControllerPitchInput(float Value) {}
	void PossessedBy(APlayerController* NewController) { Controller = NewController; }
	void UnPossessed() { Controller = nullptr; }
	APlayerController* GetController() const { return Controller; }
private:
	APlayerController* Controller = nullptr;
};
