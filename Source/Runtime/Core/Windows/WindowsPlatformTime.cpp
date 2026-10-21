#include "EnginePCH.h"
#include "WindowsPlatformTime.h"

double FWindowsPlatformTime::InitTiming()
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    SecondsPerCycle = 1.0 / (double)Frequency.QuadPart;
    SecondsPerCycle64 = 1.0 / (double)Frequency.QuadPart;
    
    return FPlatformTime::Seconds();
}
