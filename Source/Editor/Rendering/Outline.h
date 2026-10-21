#pragma once

#include "Components/PrimitiveComponent.h"

class FOutline
{
public:
	void SetTarget(UPrimitiveComponent* InTarget) { Target = InTarget; }
	UPrimitiveComponent* GetTarget() const { return Target; }
	UStaticMesh* GetMesh() const { return Target ? Target->GetRenderMesh() : nullptr; }
	FMatrix GetWorldMatrix() const { return Target->GetWorldMatrix(); }
	const FVector& GetTargetScale() const { return Target->GetRelativeScale3D(); }
private:
	UPrimitiveComponent* Target = nullptr;

};