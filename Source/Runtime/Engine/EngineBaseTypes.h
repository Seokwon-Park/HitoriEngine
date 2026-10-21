#pragma once

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Math/EngineMath.h"

class AActor;
class UActorComponent;
class FTickTaskManager;

// 같은 그룹 안에서는 등록 순서대로, 그룹끼리는 선언 순서대로 실행
enum class ETickingGroup : uint8 { PrePhysics, DuringPhysics, PostPhysics, PostUpdateWork, Max };

// UE의 FTickFunction 축소판. bCanEverTick이 켜진 함수만 FTickTaskManager에 등록되어
// 등록되지 않은 Actor·Component는 매 프레임 순회 대상에서 빠진다.
struct FTickFunction
{
public:
	enum class ETickState : uint8 { Disabled, Enabled, CoolingDown };

	FTickFunction() = default;
	// 소멸 시 자동 해제해 매니저가 해제된 포인터를 호출하지 않게 한다.
	virtual ~FTickFunction() { UnRegisterTickFunction(); }

	// 등록 상태(Manager·Index)가 복사되면 같은 슬롯을 두 객체가 가리키게 된다.
	FTickFunction(const FTickFunction&) = delete;
	FTickFunction& operator=(const FTickFunction&) = delete;

	virtual void ExecuteTick(float DeltaTime) = 0;

	void RegisterTickFunction(FTickTaskManager& InManager);
	void UnRegisterTickFunction();
	void SetTickFunctionEnable(bool bEnable);
	bool IsTickFunctionRegistered() const { return Manager != nullptr; }
	bool IsTickFunctionEnabled() const { return TickState != ETickState::Disabled; }

	uint8 bTickEvenWhenPaused : 1 = false;
	uint8 bCanEverTick : 1 = false;          // UE 기본값
	uint8 bStartWithTickEnabled : 1 = true;  // 등록할때 바로 Tick을 켤 껀지?

	ETickingGroup TickGroup = ETickingGroup::PrePhysics; // 실행 순서 그룹
	float TickInterval = 0.0f;               // 0이면 매 프레임 Tick을 몇초마다 실행할 것인가?

private:
	friend class FTickTaskManager;
	// 현재 Tick의 상태
	ETickState TickState = ETickState::Disabled;
	// 다음 Tick 실행까지 남은 시간
	float RelativeTickCooldown = 0.0f; 
	// Tick이 등록되어 있는 매니저
	FTickTaskManager* Manager = nullptr;
	// 매니저 배열 상의 위치
	int32 ManagerIndex = INDEX_NONE;          // 해제 시 O(1) RemoveAtSwap
};

struct FActorTickFunction : public FTickFunction
{
	AActor* Target = nullptr;
	void ExecuteTick(float DeltaTime) override;
};

struct FActorComponentTickFunction : public FTickFunction
{
	UActorComponent* Target = nullptr;
	void ExecuteTick(float DeltaTime) override;
};

// UWorld가 소유하며 등록된 Tick 함수만 그룹 순서대로 실행한다.
class FTickTaskManager
{
public:
	FTickTaskManager() = default;
	// 매니저가 먼저 사라져도 남은 Tick 함수가 해제 시 매니저를 건드리지 않게 연결을 끊는다.
	~FTickTaskManager();
	FTickTaskManager(const FTickTaskManager&) = delete;
	FTickTaskManager& operator=(const FTickTaskManager&) = delete;

	void AddTickFunction(FTickFunction* Function);
	void RemoveTickFunction(FTickFunction* Function);

	void RunTickGroup(ETickingGroup Group, float DeltaTime);
	void RunAllTickGroups(float DeltaTime);

	int32 GetRegisteredCount() const;

private:
	void FlushPendingRemovals();

	TArray<FTickFunction*> Groups[static_cast<uint8>(ETickingGroup::Max)];
	// Tick 실행 중 해제된 슬롯은 nullptr로만 비워 두고 그룹 순회가 끝난 뒤 정리한다.
	bool bIsTicking = false;
	bool bHasPendingRemovals = false;
};
