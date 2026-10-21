#include "EnginePCH.h"
#include "StatOverlay.h"

#include "EngineStatics.h"

#include <cctype>
#include <format>
#include <psapi.h>

#pragma comment(lib, "psapi")

namespace
{
	// 콘솔 입력은 대소문자를 구분하지 않는다. UE도 "STAT FPS"와 "stat fps"를 같게 다룬다.
	bool EqualsIgnoreCase(const FString& A, const char* B)
	{
		uint64 Index = 0;
		for (; Index < A.size() && B[Index] != '\0'; ++Index)
		{
			const int Left = std::tolower(static_cast<unsigned char>(A[Index]));
			const int Right = std::tolower(static_cast<unsigned char>(B[Index]));
			if (Left != Right)
				return false;
		}
		return Index == A.size() && B[Index] == '\0';
	}

	// 공백 구분 토큰으로 나눈다. 연속 공백과 앞뒤 공백은 무시한다.
	void Tokenize(const FString& Line, TArray<FString>& OutTokens)
	{
		OutTokens.Reset();
		uint64 Cursor = 0;
		while (Cursor < Line.size())
		{
			while (Cursor < Line.size() && std::isspace(static_cast<unsigned char>(Line[Cursor])))
				++Cursor;
			const uint64 Start = Cursor;
			while (Cursor < Line.size() && !std::isspace(static_cast<unsigned char>(Line[Cursor])))
				++Cursor;
			if (Cursor > Start)
				OutTokens.Add(Line.substr(Start, Cursor - Start));
		}
	}
}

// 샘플 구간이 찰 때만 표시 수치를 바꿔 숫자가 매 프레임 튀지 않게 한다.
void FStatOverlay::Tick(const float DeltaTime)
{
	AccumulatedTime += DeltaTime;
	++AccumulatedFrames;

	if (AccumulatedTime < SampleInterval || AccumulatedFrames <= 0)
		return;

	DisplayFPS = static_cast<float>(AccumulatedFrames) / AccumulatedTime;
	DisplayFrameTimeMs = (AccumulatedTime / static_cast<float>(AccumulatedFrames)) * 1000.0f;

	AccumulatedTime = 0.0f;
	AccumulatedFrames = 0;
}

// stat 명령만 소비하고 나머지는 호출자에게 되돌린다.
bool FStatOverlay::ExecCommand(const FString& CommandLine, FString& OutMessage)
{
	TArray<FString> Tokens;
	Tokenize(CommandLine, Tokens);

	if (Tokens.Num() == 0 || !EqualsIgnoreCase(Tokens[0], "stat"))
		return false;

	if (Tokens.Num() < 2)
	{
		OutMessage = "Usage: stat <fps|memory|profile|all|none>";
		return true;
	}

	const FString& Arg = Tokens[1];

	if (EqualsIgnoreCase(Arg, "none"))
	{
		Flags = EStatFlags::None;
		OutMessage = "stat none: all overlays disabled";
		return true;
	}

	if (EqualsIgnoreCase(Arg, "all"))
	{
		Flags = EStatFlags::FPS | EStatFlags::Memory | EStatFlags::Profile;
		OutMessage = "stat all: all overlays enabled";
		return true;
	}

	EStatFlags Target = EStatFlags::None;
	if (EqualsIgnoreCase(Arg, "fps"))
		Target = EStatFlags::FPS;
	else if (EqualsIgnoreCase(Arg, "memory"))
		Target = EStatFlags::Memory;
	else if (EqualsIgnoreCase(Arg, "profile"))
		Target = EStatFlags::Profile;

	if (Target == EStatFlags::None)
	{
		OutMessage = std::format("stat: unknown stat '{}'. Available: fps, memory, profile, all, none", Arg);
		return true;
	}

	// UE와 같이 같은 명령을 다시 입력하면 해당 항목만 꺼진다.
	const bool bEnable = !HasFlag(Flags, Target);
	if (bEnable)
		Flags |= Target;
	else
		Flags &= ~Target;

	OutMessage = std::format("stat {}: {}", Arg, bEnable ? "on" : "off");
	return true;
}

uint64 FStatOverlay::GetObjectAllocationBytes()
{
	return FEngineStatics::TotalAllocationBytes;
}

uint32 FStatOverlay::GetObjectAllocationCount()
{
	return FEngineStatics::TotalAllocationCount;
}

// UObject 추적치는 엔진이 직접 센 값이라 텍스처·버퍼 같은 다른 할당이 빠진다.
// 전체 사용량은 OS에 직접 묻는다.
uint64 FStatOverlay::GetProcessWorkingSetBytes()
{
	PROCESS_MEMORY_COUNTERS Counters{};
	if (!GetProcessMemoryInfo(GetCurrentProcess(), &Counters, sizeof(Counters)))
		return 0;
	return static_cast<uint64>(Counters.WorkingSetSize);
}
