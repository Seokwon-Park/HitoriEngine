#pragma once

struct FWindowsPlatformTime
{
	static double InitTiming();

	static inline double Seconds()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);

		return (double)Cycles.QuadPart * GetSecondsPerCycle() + GetSecondsTimeOffset();
	}

	static inline uint32 GetCycles()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return (uint32)Cycles.QuadPart;
	}

	static inline uint64 GetCycles64()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return Cycles.QuadPart;
	}

	// 원래 FGenericPlatformTime에 있어야 하는 함수이지만 현 상황에서는 멀티플랫폼을 고려하지 않는다.
	static double GetSecondsPerCycle()
	{
		return SecondsPerCycle;
	}

	static float ToMilliseconds(const uint32 Cycles)
	{
		return (float)double(SecondsPerCycle * 1000.0 * Cycles);
	}

	static float ToSeconds(const uint32 Cycles)
	{
		return (float)double(SecondsPerCycle * Cycles);
	}

	static double GetSecondsPerCycle64()
	{
		return SecondsPerCycle64;
	}

	static double ToMilliseconds64(const uint64 Cycles)
	{
		return ToSeconds64(Cycles) * 1000.0;
	}

	static double ToSeconds64(const uint64 Cycles)
	{
		return GetSecondsPerCycle64() * double(Cycles);
	}

	static uint64 SecondsToCycles64(double Seconds)
	{
		return static_cast<uint64>(Seconds / GetSecondsPerCycle64());
	}

	static inline double GetSecondsTimeOffset()
	{
		return 16777216.0;
	}

private:

	// 원래 FGenericPlatformTime에 있어야 하지만 현 상황에서는 멀티플랫폼을 고려하지 않는다.
	static inline double SecondsPerCycle;
	static inline double SecondsPerCycle64;
	//static double LastIntervalCPUTimeInSeconds;
};

using FPlatformTime = FWindowsPlatformTime;