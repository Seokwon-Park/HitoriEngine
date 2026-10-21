#include "EnginePCH.h"
#include "Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/SceneComponent.h"
#include "Components/ParticleSubUVComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Core/EngineLog.h"
#include "UObject/Object.h"

enum class EAttachmentRule;


AActor::AActor()
{
	PrimaryActorTick.Target = this;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("DefaultSceneRoot"));
}

AActor::~AActor()
{
	TArray<UActorComponent*> ToDelete = Components;
	Components.Reset();
	RootComponent = nullptr;

	for (UActorComponent* Component : ToDelete)
	{
		delete Component;
	}
}

void AActor::BeginPlay()
{
	if (bHasBegunPlay || !World || !World->HasBegunPlay()) return;
	RegisterAllComponents();
	bHasBegunPlay = true;
	for (UActorComponent* Component : Components)
		if (Component) Component->BeginPlayComponent();
	RegisterAllActorTickFunctions(true);
}

void AActor::RegisterAllActorTickFunctions(bool bRegister)
{
	if (bRegister && !World)
		return;

	// bCanEverTick이 꺼진 함수는 등록하지 않으므로 정적 메시 액터는 매 프레임 순회 대상에서 빠진다.
	auto Apply = [&](FTickFunction& Function)
		{
			if (bRegister)
				Function.RegisterTickFunction(World->GetTickTaskManager());
			else
				Function.UnRegisterTickFunction();
		};

	Apply(PrimaryActorTick);
	for (UActorComponent* Component : Components)
	{
		if (Component)
			Component->RegisterComponentTickFunctions(bRegister);
	}
}

void AActor::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // "Properties"

	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		int32 Num = Components.Num();
		FStructuredArchive::FArray ActorArray = Record.EnterArray("Components", Num);
		if (Ar.IsLoading())
			Components.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			UObject* Obj = Components[i];
			ActorArray.EnterElement() << Obj;
			Components[i] = static_cast<UActorComponent*>(Obj);
		}

		UObject* WorldObj = World;
		UObject* LevelObj = Level;
		UObject* RootObj = RootComponent;
		Record << SA_VALUE("OwningWorld", WorldObj);
		Record << SA_VALUE("OwningLevel", LevelObj);
		Record << SA_VALUE("RootComponent", RootObj);
		World = static_cast<UWorld*>(WorldObj);
		Level = static_cast<ULevel*>(LevelObj);
		RootComponent = static_cast<USceneComponent*>(RootObj);
		return;
	}

	TArray<UActorComponent*> SavedComponents;
	if (Ar.IsSaving())
	{
		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				SavedComponents.Add(Component);
			}

		}
	}

	struct FPendingAttachment
	{
		USceneComponent* Child;
		FString ParentName;
	};
	TArray<FPendingAttachment> PendingAttachments;

	int32 Num = SavedComponents.Num();
	FStructuredArchive::FArray ComponentArray = Record.EnterArray("Components", Num);
	for (int32 i = 0; i < Num; ++i)
	{
		FStructuredArchive::FRecord ComponentRecord = ComponentArray.EnterElement().EnterRecord();
		FString ParentName;

		if (Ar.IsSaving())
		{
			UActorComponent* Component = SavedComponents[i];
			FString Name = Component->GetName();
			FString ClassName = Component->GetClass()->Name;
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				ParentName = SceneComponent->GetAttachParent() ? SceneComponent->GetAttachParent()->GetName() : "";
			}
			ComponentRecord << SA_VALUE("Name", Name) << SA_VALUE("Class", ClassName) << SA_VALUE("Parent", ParentName);
			Component->Serialize(ComponentRecord);
			continue;
		}

		FString Name, ClassName;
		ComponentRecord << SA_VALUE("Name", Name) << SA_VALUE("Class", ClassName) << SA_VALUE("Parent", ParentName);

		UActorComponent* Component = FindComponentByName(FName(Name));
		if (!Component)
		{
			UClass* ComponentClass = FindClass(ClassName);

			if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				HTR_LOG(Warning, "Load: unknown component class '{}'", ClassName);
				continue;
			}

			Component = AddComponent(ComponentClass, FName(Name));
		}

		if (!Component || Component->GetClass()->Name != ClassName)
		{
			HTR_LOG(Warning, "Load: component {} ({}) failed", Name, ClassName);
			continue;
		}

		Component->Serialize(ComponentRecord);
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (!ParentName.empty())
			{
				PendingAttachments.Add({ SceneComponent, ParentName });
			}
		}
	}

	for (const FPendingAttachment& Attachment : PendingAttachments)
	{
		USceneComponent* ParentComponent = Cast<USceneComponent>(FindComponentByName(FName(Attachment.ParentName)));
		if (ParentComponent)
		{
			Attachment.Child->SetupAttachment(ParentComponent);
		}
	}
}


void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	for (uint32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == Component)
		{
			Components.RemoveAt(i, 1);
			break;
		}
	}

	if (RootComponent == Component)
	{
		RootComponent = nullptr;
	}
}

FVector AActor::GetActorLocation() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldLocation();
	}
	return FVector::ZeroVector;
}

FRotator AActor::GetActorRotation() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldRotation();
	}
	return FRotator();
}

FVector AActor::GetActorScale3D() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldScale3D();
	}
	return FVector::OneVector;
}

FQuat AActor::GetActorQuat() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldRotation().Quaternion();
	}
	return FQuat::Identity();
}

FTransform AActor::GetActorTransform() const
{
	if (RootComponent)
	{
		return FTransform(
			RootComponent->GetWorldRotation(),
			RootComponent->GetWorldLocation(),
			RootComponent->GetWorldScale3D()
		);

		// return FTransform(RootComponent->GetWorldMatrix());
	}
	return FTransform::Identity;
}

bool AActor::Destroy()
{
	if (!World)
		return false;

	return World->DestroyActor(this);
}

UActorComponent* AActor::AddComponent(UClass* ComponentClass, FName Name)
{
	if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return nullptr;
	}


	if (FName(Name) == FName("None"))
	{
		Name = MakeUniqueObjectName(this, ComponentClass);
	}

	UActorComponent* NewComponent = NewObject<UActorComponent>(this, ComponentClass, Name);
	if (!NewComponent){return nullptr;}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(NewComponent))
	{
		SceneComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	}

	NewComponent->SetOwner(this);
	Components.Add(NewComponent);
	NewComponent->RegisterComponent();

	return NewComponent;
}

void AActor::DestroyComponent(UActorComponent* Component)
{
	if (!Component)
		return;

	if (Component == RootComponent)
	{
		HTR_LOG(Warning, "DefaultSceneRoot cannot be deleted.");
		return;
	}

	if (USceneComponent* Parent = Cast<USceneComponent>(Component))
	{
		TArray<USceneComponent*> Children = Parent->GetAttachChildren();
		for (USceneComponent* Candidate : Children)
		{
			if (!Candidate) { continue; }

			Candidate->DetachFromParent(EAttachmentRule::KeepWorld);
			Candidate->SetupAttachment(Parent->GetAttachParent(), EAttachmentRule::KeepWorld);
		}
	}

	Component->UnregisterComponent();
	RemoveOwnedComponent(Component);
	delete Component;
	return;
}

void AActor::OnDefaultSubobjectCreated(UObject* Subobject)
{
	if (UActorComponent* Component = Cast<UActorComponent>(Subobject))
	{
		Component->SetOwner(this);
		Components.Add(Component);
	}
}

UActorComponent* AActor::FindComponentByName(FName Name) const
{
	for (UActorComponent* Component : Components)
		if (Component && Component->GetFName() == Name)
			return Component;
	return nullptr;
}

void AActor::RegisterAllComponents()
{
	if (!World) return;

	for (UActorComponent* Component : Components)
	{
		if (Component && !Component->IsRegistered())
			Component->RegisterComponentWithWorld(World);
	}
}

void AActor::UnregisterAllComponents()
{
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->IsRegistered())
			Component->UnregisterComponent();
	}
}

bool AActor::TryGetActorBounds(FBox& OutBounds) const
{
	bool bHasBounds = false;

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive)
			continue;

		FBox PrimitiveBounds = Primitive->CalcBounds();
		if (bHasBounds)
		{
			OutBounds.Expand(PrimitiveBounds);
		}
		else
		{
			OutBounds = PrimitiveBounds;
			bHasBounds = true;
		}
	}

	return bHasBounds;
}
