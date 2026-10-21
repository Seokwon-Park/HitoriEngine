#include "EnginePCH.h"
#include "EngineTimer.h"

void EngineTimer::Init()
{
	PrevTime = Clock::now();
}

void EngineTimer::Tick()
{
	Clock::time_point Current = Clock::now();
	std::chrono::duration<float> Elapsed = Current - PrevTime;

	DeltaTime = Elapsed.count();
	PrevTime = Current;

	if (DeltaTime > 0.1f) DeltaTime = 0.1f;
	TotalTime += DeltaTime;
}
