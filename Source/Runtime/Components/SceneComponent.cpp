#include "EnginePCH.h"
#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	void UpdateChildVisibility(USceneComponent* Root, bool bPropagate, bool bVisible, bool bHidden)
	{
		TArray<USceneComponent*> Pending = Root->GetAttachChildren();
		while (!Pending.IsEmpty())
		{
			USceneComponent* Child = Pending[Pending.Num() - 1];
			Pending.RemoveLast();
			if (!Child) continue;
			if (bPropagate)
			{
				if (bVisible) Child->UActorComponent::SetVisibility(Root->IsVisible());
				if (bHidden) Child->UActorComponent::SetHiddenInGame(Root->IsHiddenInGame());
			}
			Child->MarkRenderStateDirty();
			for (USceneComponent* Descendant : Child->GetAttachChildren()) Pending.Add(Descendant);
		}
	}
}

USceneComponent::~USceneComponent()
{
	TArray<USceneComponent*> Children = AttachChildren;
	DetachFromParent();
	for (USceneComponent* Child : Children)
	{
		Child->AttachParent = nullptr;
	}
	AttachChildren.Reset();

	DetachFromParent();
}

void USceneComponent::SetRelativeLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	Transform.Location = NewLocation;
	Transform.Rotation = NewRotation;
	MarkTransformDirty();
}

void USceneComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "Transform") 
		MarkTransformDirty();
	else if (Property.Name == "bVisible" || Property.Name == "bHiddenInGame")
		UpdateChildVisibility(this, false, false, false);
}

bool USceneComponent::IsShown(bool bGameView) const
{
	for (const USceneComponent* Component = this; Component; Component = Component->AttachParent)
		if (!Component->UActorComponent::IsShown(bGameView)) return false;
	return true;
}

void USceneComponent::SetVisibility(bool bNewVisibility, EVisibilityPropagation Propagation)
{
	const bool bChanged = bVisible != bNewVisibility;
	Super::SetVisibility(bNewVisibility);
	if (bChanged || Propagation == EVisibilityPropagation::Propagate)
		UpdateChildVisibility(this, Propagation == EVisibilityPropagation::Propagate, true, false);
}

void USceneComponent::SetHiddenInGame(bool bNewHidden, EVisibilityPropagation Propagation)
{
	const bool bChanged = bHiddenInGame != bNewHidden;
	Super::SetHiddenInGame(bNewHidden);
	if (bChanged || Propagation == EVisibilityPropagation::Propagate)
		UpdateChildVisibility(this, Propagation == EVisibilityPropagation::Propagate, false, true);
}

void USceneComponent::SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule)
{
	if (InParent == this || AttachParent == InParent) return;

	// 순환 체크
	for (USceneComponent* Parent = InParent; Parent != nullptr; Parent = Parent->AttachParent)
		if (Parent == this) return;

	if (Rule == EAttachmentRule::KeepWorld)
	{
		FMatrix WorldMatrix = GetWorldMatrix();
		DetachFromParent(Rule);
		AttachParent = InParent;
		if (AttachParent)
		{
			AttachParent->AttachChildren.Add(this);
		}
		SetWorldTransform(WorldMatrix, false);
	}
	else
	{
		DetachFromParent(Rule);
		AttachParent = InParent;
		if (AttachParent)
		{
			AttachParent->AttachChildren.Add(this);
		}
	}

	if (Rule == EAttachmentRule::SnapToTarget)
	{
		SetTransform(FTransform::Identity);
	}

	MarkTransformDirty();
	MarkRenderStateDirty();
	UpdateChildVisibility(this, false, false, false);
}

void USceneComponent::DetachFromParent(EAttachmentRule Rule)
{
	if (!AttachParent) return;

	if (Rule == EAttachmentRule::KeepWorld)
	{
		FMatrix WorldMatrix = GetWorldMatrix();
		TArray<USceneComponent*>& Siblings = AttachParent->AttachChildren;
		for (uint32 i = 0;i < Siblings.Num(); ++i)
		{
			if (Siblings[i] == this)
			{
				Siblings.RemoveAt(i, 1);
				break;
			}
		}
		AttachParent = nullptr;
		SetWorldTransform(WorldMatrix, false);
	}
	else
	{
		TArray<USceneComponent*>& Siblings = AttachParent->AttachChildren;
		for (uint32 i = 0;i < Siblings.Num(); ++i)
		{
			if (Siblings[i] == this)
			{
				Siblings.RemoveAt(i, 1);
				break;
			}
		}
		AttachParent = nullptr;
	}
	
	MarkTransformDirty();
	MarkRenderStateDirty();
	UpdateChildVisibility(this, false, false, false);
}

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

		return FVector(
			Transform.Scale.X * ParentScale.X,
			Transform.Scale.Y * ParentScale.Y,
			Transform.Scale.Z * ParentScale.Z
		);
	}

	return Transform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	if (!AttachParent)
	{
		return Transform.GetLocalMatrix();
	}

	const FVector WorldLocation = 
		AttachParent->GetWorldMatrix().TransformPosition(Transform.Location);

	const FQuat WorldRotation = 
		AttachParent->GetWorldRotation().Quaternion() * Transform.GetOrientation();

	const FVector WorldScale =
		Transform.Scale * AttachParent->GetWorldScale3D();

	return FTransform(WorldRotation.ToFRotator(), WorldLocation, WorldScale).GetLocalMatrix();
}

void USceneComponent::SetWorldTransform(const FMatrix& InWorldMatrix, bool bSweep)
{
	FMatrix LocalMatrix = InWorldMatrix;
	if (AttachParent)
	{
		FMatrix ParentWorldMatrix = AttachParent->GetWorldMatrix();
		LocalMatrix = InWorldMatrix * ParentWorldMatrix.Inverse();
	}

	Transform.Location = FVector(LocalMatrix[3][0], LocalMatrix[3][1], LocalMatrix[3][2]);

	FVector XAxis(LocalMatrix[0][0], LocalMatrix[0][1], LocalMatrix[0][2]);
	FVector YAxis(LocalMatrix[1][0], LocalMatrix[1][1], LocalMatrix[1][2]);
	FVector ZAxis(LocalMatrix[2][0], LocalMatrix[2][1], LocalMatrix[2][2]);

	const float XAxisSize = XAxis.Size();
	const float YAxisSize = YAxis.Size();
	const float ZAxisSize = ZAxis.Size();

	Transform.Scale = FVector(XAxisSize, YAxisSize, ZAxisSize);

	XAxis = XAxis.Normalized();
	YAxis = YAxis.Normalized();
	ZAxis = ZAxis.Normalized();

	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.SetAxis(0, XAxis);
	RotationMatrix.SetAxis(1, YAxis);
	RotationMatrix.SetAxis(2, ZAxis);

	Transform.Rotation = MatrixToRotator(RotationMatrix);
		
	MarkTransformDirty();
}

void USceneComponent::SetWorldLocation(FVector NewLocation, bool bSweep)
{
	FVector NewRelLocation = NewLocation;

	if (GetAttachParent() != nullptr)
	{		
		NewRelLocation = GetAttachParent()->GetWorldMatrix().Inverse().TransformPosition(NewRelLocation);
	}

	SetRelativeLocation(NewRelLocation);
}

void USceneComponent::SetWorldRotation(FRotator NewRotation, bool bSweep)
{
	FRotator NewRelRotation = NewRotation;

	if (GetAttachParent() != nullptr)
	{
		FQuat NewRelQuat = GetAttachParent()->GetWorldRotation().Quaternion().Inverse() * NewRelRotation.Quaternion();
		NewRelRotation = NewRelQuat.ToFRotator();
	}

	SetRelativeRotation(NewRelRotation);
}

void USceneComponent::SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep)
{
	FVector NewRelLocation = NewLocation;
	FRotator NewRelRotation = NewRotation;

	if (GetAttachParent() != nullptr)
	{
		NewRelLocation = GetAttachParent()->GetWorldMatrix().Inverse().TransformPosition(NewRelLocation);

		FQuat NewRelQuat = GetAttachParent()->GetWorldRotation().Quaternion().Inverse() * NewRelRotation.Quaternion();
		NewRelRotation = NewRelQuat.ToFRotator();
	}

	SetRelativeLocationAndRotation(NewRelLocation, NewRelRotation);
}

void USceneComponent::SetWorldScale3D(FVector NewScale)
{
}

bool USceneComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	FHitResult LocalHit(1.0f);
	FHitResult& Result = Hit ? *Hit : LocalHit;
	Result = FHitResult(1.0f);
	const FVector Start = GetWorldLocation();
	FVector Destination = Start + Delta;
	bool bBlocked = false;
	AActor* Owner = GetOwner();
	if (bSweep && Delta.SizeSquared() > SMALL_NUMBER && Owner && Owner->GetActorEnableCollision())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			Result.Init(Start, Destination);
			bBlocked = World->SweepSingle(FRay(Start, Delta.Normalized()), Delta.Size(), GetCollisionRadius(), Result, Owner);
			if (bBlocked)
				Destination = Start + Delta * Result.Time + Result.ImpactNormal * 0.001f;
		}
	}
	SetWorldLocationAndRotation(Destination, NewRotation, false);
	return !bBlocked;
}

float USceneComponent::GetCollisionRadius() const
{
	if (CollisionRadiusOverride >= 0.0f) return CollisionRadiusOverride;
	const FBox Bounds = CalcBounds();
	const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
	if (Extent.SizeSquared() <= SMALL_NUMBER) return 10.0f;
	return std::min({ Extent.X, Extent.Y, Extent.Z });
}

void USceneComponent::MarkTransformDirty()
{
	OnTransformDirty();         

	for (USceneComponent* Child : AttachChildren)
		Child->MarkTransformDirty();      // 부모가 움직이면 자식의 월드 행렬도 바뀐다
}

void USceneComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // UActorComponent: Owner까지 처리됨

	FArchive& Ar = Record.GetUnderlyingArchive();
	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		UObject* ParentObj = AttachParent;
		Record << SA_VALUE("AttachParent", ParentObj);
		AttachParent = static_cast<USceneComponent*>(ParentObj);

		int32 Num = AttachChildren.Num();
		FStructuredArchive::FArray ChildArray = Record.EnterArray("AttachChildren", Num);
		if (Ar.IsLoading())
			AttachChildren.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			UObject* Obj = AttachChildren[i];
			ChildArray.EnterElement() << Obj;
			AttachChildren[i] = static_cast<USceneComponent*>(Obj);
		}
	}
}



