#pragma once

#include <chrono>


class EngineTimer
{
public:
	static void Init();
	static void Tick();

	static float GetDeltaTime() { return DeltaTime; };
	static float GetTotalTime() { return TotalTime; }
private:
	using Clock = std::chrono::high_resolution_clock;

	inline static Clock::time_point PrevTime;
	inline static float DeltaTime = 0.0f;
	inline static float TotalTime = 0.0f;
};
