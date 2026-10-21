#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 0;
	Desc.Height = 0;
	Desc.bBorderless = true;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = true;   
	Desc.bExitOnEscape = true;        
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init()) return false;

	return true;
}

void UGameEngine::Tick(float DeltaTime)
{
	//World->Tick(DeltaTime);

}

