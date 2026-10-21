#pragma once

#include "../Math/Transform.h"
#include "ActorComponent.h"
#include "Math/Box.h"
#include "Collision/HitResult.h"

enum class EAttachmentRule
{
	KeepRelative,
	KeepWorld,
	SnapToTarget
};

class USceneComponent : public UActorComponent
{
	DECLARE_CLASS(USceneComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY(Transform)
		REFLECT_END()

public:
	enum class EVisibilityPropagation : uint8 { NoPropagation, DirtyOnly, Propagate };

	USceneComponent() = default;
	virtual ~USceneComponent() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// Get & Set
	const FVector& GetRelativeLocation() const { return Transform.Location; }
	void SetRelativeLocation(const FVector& InLocation) 
	{
		Transform.Location = InLocation;
		MarkTransformDirty();
	}


	const FRotator& GetRelativeRotation() const { return Transform.Rotation; }
	void SetRelativeRotation(const FRotator& InRotation) 
	{
		Transform.Rotation = InRotation; 
		MarkTransformDirty();
	}
	void SetRelativeLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
	const FVector& GetRelativeScale3D() const { return Transform.Scale; }
	void SetRelativeScale3D(const FVector& InScale) 
	{
		Transform.Scale = InScale;
		MarkTransformDirty();
	}

	// 쿼터니언 적용된 회전행렬
	FQuat GetRelativeRotationQuat() const { return Transform.GetOrientation(); }

	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform) 
	{
		Transform = InTransform;
		MarkTransformDirty();
	}

	// Attatch-To
	USceneComponent* GetAttachParent() const { return AttachParent; }
	const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }
	void SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule = EAttachmentRule::KeepRelative);
	void DetachFromParent(EAttachmentRule Rule = EAttachmentRule::KeepRelative);

	virtual FBox CalcLocalBounds() const { return FBox{ FVector(), FVector() }; }
	FBox CalcBounds() const { return CalcLocalBounds().GetWorldAABB(GetWorldMatrix()); }

	FVector GetWorldLocation() const;
	FRotator GetWorldRotation() const;
	FVector GetWorldScale3D() const;
	FMatrix GetWorldMatrix() const;
	
	void SetWorldLocation(FVector NewLocation, bool bSweep);
	void SetWorldRotation(FRotator NewRotation, bool bSweep);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep);
	void SetWorldScale3D(FVector NewScale);	
	void SetWorldTransform(const FMatrix& InWorldMatrix, bool bSweep);

	virtual bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit = nullptr);
	virtual float GetCollisionRadius() const;
	void SetCollisionRadiusOverride(float Radius) { CollisionRadiusOverride = Radius; }

	void MarkTransformDirty();
	virtual void OnTransformDirty() {};

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual bool IsShown(bool bGameView) const override;
	void SetVisibility(bool bNewVisibility) override
	{ SetVisibility(bNewVisibility, EVisibilityPropagation::NoPropagation); }
	void SetVisibility(bool bNewVisibility, EVisibilityPropagation Propagation);
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren)
	{ SetVisibility(bNewVisibility, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::NoPropagation); }
	void SetHiddenInGame(bool bNewHidden) override
	{ SetHiddenInGame(bNewHidden, EVisibilityPropagation::NoPropagation); }
	void SetHiddenInGame(bool bNewHidden, EVisibilityPropagation Propagation);
	void SetHiddenInGame(bool bNewHidden, bool bPropagateToChildren)
	{ SetHiddenInGame(bNewHidden, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::NoPropagation); }
	virtual void MarkRenderStateDirty() override {}

public:
	FVector ComponentVelocity;

protected:

	bool bTransformDirty;
	FTransform Transform;

	USceneComponent* AttachParent = nullptr; // Attach 부모 정보
	TArray<USceneComponent*> AttachChildren;

	bool bAbsoluteLocation = false;
	bool bAbsoluteRotation = false;
	bool bAbsoluteScale = false;
	float CollisionRadiusOverride = -1.0f;
};
