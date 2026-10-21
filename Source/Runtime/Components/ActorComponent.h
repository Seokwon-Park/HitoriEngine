#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/EngineBaseTypes.h"

class AActor;
class UWorld;

class UActorComponent : public UObject
{
	DECLARE_CLASS(UActorComponent, UObject)

	REFLECT_START(ClassName)
		PROPERTY(bAutoActivate)
		PROPERTY(bVisible)
		PROPERTY(bHiddenInGame)
		REFLECT_END()

public:
	UActorComponent() { PrimaryComponentTick.Target = this; }
	virtual ~UActorComponent() override;

	virtual void InitializeComponent() { bIsInitialized = true; }
	virtual void BeginPlay() {};
	void BeginPlayComponent();
	bool HasBegunPlay() const { return bHasBegunPlay; }
	virtual void TickComponent(float DeltaTime)
	{
	};

	void SetOwner(AActor* InOwner) { Owner = InOwner; }
	AActor* GetOwner() const { return Owner; }

	// 디테일 패널에서 이 컴포넌트를 통째로 숨긴다 (에디터 시각화용 빌보드 등)
	void SetHiddenInDetails(bool bInHidden) { bHiddenInDetails = bInHidden; }
	bool IsHiddenInDetails() const { return bHiddenInDetails; }

	// UE와 같이 기본값은 bCanEverTick = false. Tick이 필요한 컴포넌트만 생성자에서 켠다.
	FActorComponentTickFunction PrimaryComponentTick;

	virtual void Activate(bool bReset = false);
	virtual void Deactivate();
	virtual bool ShouldActivate() const { return !bIsActive; }
	virtual void SetActive(bool bNewActive);
	bool IsActive() const { return bIsActive; }
	void SetAutoActivate(bool bNewAutoActivate);
	bool IsAutoActivate() const { return bAutoActivate; }
	bool IsVisible() const { return bVisible; }
	bool IsHiddenInGame() const { return bHiddenInGame; }
	virtual bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	virtual void SetVisibility(bool bNewVisibility);
	virtual void SetHiddenInGame(bool bNewHidden);
	virtual void MarkRenderStateDirty() {}
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	void SetComponentTickEnabled(bool bEnabled);
	bool IsComponentTickEnabled() const { return PrimaryComponentTick.bCanEverTick && PrimaryComponentTick.IsTickFunctionEnabled(); }
	void RegisterComponentTickFunctions(bool bRegister);

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record);

	void RegisterComponent();
	void RegisterComponentWithWorld(UWorld* InWorld);
	void UnregisterComponent();

	bool IsRegistered() const { return bRegistered; }
	bool IsRenderStateCreated() const { return bRenderStateCreated; }

	UWorld* GetWorld() const { return WorldPrivate; }
protected:

	virtual void OnRegister();
	virtual void OnUnregister();
	virtual bool ShouldCreateRenderState() const
	{
		return false;
	}
	
	//원래는 _Concurrent 병렬처리 -> 현재 엔진에서는 멀티 스레드 사용 X 
	virtual void CreateRenderState();
	virtual void DestroyRenderState();
	bool bAutoActivate = true;
	bool bVisible = true;
	bool bHiddenInGame = false;

	void ExecuteRegisterEvents();
	void ExecuteUnregisterEvents();

private:
	AActor* Owner = nullptr;
	bool bHiddenInDetails = false;
	bool bIsActive = false;
	bool bRegistered = false;
	bool bIsInitialized = false;
	bool bHasBegunPlay = false;

	UWorld* WorldPrivate = nullptr;
	bool bRenderStateCreated = false;
};
