#include "EnginePCH.h"
#include "EngineBaseTypes.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

void FTickFunction::RegisterTickFunction(FTickTaskManager& InManager)
{
	if (!bCanEverTick || Manager == &InManager)
		return;

	//다른 매니저에 등록되어 있다면 빼기
	UnRegisterTickFunction();
	//배열에 추가하고 Index 저장
	InManager.AddTickFunction(this);
	//Enabled 상태 설정
	SetTickFunctionEnable(bStartWithTickEnabled);
}

void FTickFunction::UnRegisterTickFunction()
{
	if (Manager)
		Manager->RemoveTickFunction(this);
}

void FTickFunction::SetTickFunctionEnable(bool bEnable)
{
	TickState = bEnable ? ETickState::Enabled : ETickState::Disabled;
	RelativeTickCooldown = TickInterval;
}

void FActorTickFunction::ExecuteTick(float DeltaTime)
{
	if (Target)
		Target->TickActor(DeltaTime);
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime)
{
	if (Target)
		Target->TickComponent(DeltaTime);
}

FTickTaskManager::~FTickTaskManager()
{
	for (TArray<FTickFunction*>& Functions : Groups)
	{
		for (FTickFunction* Function : Functions)
		{
			if (!Function) continue;
			Function->Manager = nullptr;
			Function->ManagerIndex = INDEX_NONE;
		}
	}
}

void FTickTaskManager::AddTickFunction(FTickFunction* Function)
{
	TArray<FTickFunction*>& Functions = Groups[static_cast<uint8>(Function->TickGroup)];
	Function->Manager = this;
	Function->ManagerIndex = static_cast<int32>(Functions.Add(Function));
}

void FTickTaskManager::RemoveTickFunction(FTickFunction* Function)
{
	TArray<FTickFunction*>& Functions = Groups[static_cast<uint8>(Function->TickGroup)];
	const int32 Index = Function->ManagerIndex;

	Function->Manager = nullptr;
	Function->ManagerIndex = INDEX_NONE;
	Function->TickState = FTickFunction::ETickState::Disabled;

	if (Index < 0 || Index >= Functions.Num() || Functions[Index] != Function)
		return;

	if (bIsTicking)
	{
		// 순회 중 RemoveAtSwap은 아직 실행하지 않은 함수를 건너뛰게 만든다.
		Functions[Index] = nullptr;
		bHasPendingRemovals = true;
		return;
	}

	Functions.RemoveAtSwap(static_cast<uint32>(Index));
	if (Index < Functions.Num() && Functions[Index])
		Functions[Index]->ManagerIndex = Index;
}

void FTickTaskManager::RunTickGroup(ETickingGroup Group, float DeltaTime)
{
	TArray<FTickFunction*>& Functions = Groups[static_cast<uint8>(Group)];

	bIsTicking = true;
	// Tick 도중 새로 등록된 함수는 다음 프레임부터 실행한다.
	const int32 Count = Functions.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		FTickFunction* Function = Functions[i];
		if (!Function || Function->TickState == FTickFunction::ETickState::Disabled)
			continue;

		float TickDelta = DeltaTime;
		if (Function->TickInterval > 0.0f)
		{
			Function->RelativeTickCooldown -= DeltaTime;
			if (Function->RelativeTickCooldown > 0.0f)
			{
				Function->TickState = FTickFunction::ETickState::CoolingDown;
				continue;
			}
			// 간격 단위로 한 번 실행하며 누적된 시간을 한 번에 넘긴다.
			TickDelta = Function->TickInterval - Function->RelativeTickCooldown;
			Function->RelativeTickCooldown = Function->TickInterval;
			Function->TickState = FTickFunction::ETickState::Enabled;
		}

		Function->ExecuteTick(TickDelta);
	}
	bIsTicking = false;

	if (bHasPendingRemovals)
		FlushPendingRemovals();
}

void FTickTaskManager::RunAllTickGroups(float DeltaTime)
{
	for (uint8 Group = 0; Group < static_cast<uint8>(ETickingGroup::Max); ++Group)
		RunTickGroup(static_cast<ETickingGroup>(Group), DeltaTime);
}

int32 FTickTaskManager::GetRegisteredCount() const
{
	int32 Count = 0;
	for (const TArray<FTickFunction*>& Functions : Groups)
		Count += Functions.Num();
	return Count;
}

void FTickTaskManager::FlushPendingRemovals()
{
	// 한 Tick에서 다른 그룹의 함수가 해제될 수도 있으므로 모든 그룹을 정리한다.
	for (TArray<FTickFunction*>& Group : Groups)
	{
		int32 Write = 0;
		for (int32 Read = 0; Read < Group.Num(); ++Read)
		{
			FTickFunction* Function = Group[Read];
			if (!Function) continue;
			Function->ManagerIndex = Write;
			Group[Write++] = Function;
		}
		Group.SetNum(Write, false);
	}
	bHasPendingRemovals = false;
}
