#pragma once

#include "EnginePCH.h"
#include <Windows.h>
#include "LaunchEngineLoop.h"

// 실행 파일이 어떤 엔진 모드(UEngine 서브클래스)로 돌지 정한다. UE의 ini GameEngine= 설정 역할.
extern UClass* GetEngineClass();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	FEngineLoop EngineLoop;
	if (!EngineLoop.PreInit(hInstance, GetEngineClass())) return -1;

	while (EngineLoop.Tick())
	{
	}

	EngineLoop.Exit();

	if (FEngineStatics::TotalAllocationCount > 0)
		OutputDebugStringA("leaked UObjects\n");

	return 0;
}
