#include "EnginePCH.h"
#include "Launch/LaunchEngineLoop.h"

#include "Asset/AssetManager.h"
#include "Core/EngineTimer.h"
#include "Core/Window.h"
#include "Core/Windows/WindowsPlatformTime.h"
#include "Input/InputSystem.h"
#include "UObject/Casts.h"
#include "UObject/Class.h"
#include "Render/RenderCommand.h"
#include "Render/RenderDevice.h"
#include "Render/RenderResourceManager.h"
#include "Render/Renderer.h"
#include "Render/Swapchain.h"
#include "Render/Texture2D.h"
#include "Core/SplashScreen.h"
#include "Core/Stats/LightweightStats.h"
#include "Core/Async/TaskPool.h"

#include "UObject/UObjectGlobals.h"

namespace
{
	// CPU Frame이 Present보다 크면 CPU 병목, Present가 크면 CPU가 GPU를 기다리는 GPU 병목이다.
	DECLARE_CYCLE_STAT("Present", STAT_Present);
	DECLARE_CYCLE_STAT("CPU Frame (Engine Tick)", STAT_EngineTick);

	// 이 간격마다 FPS 평균을 갱신한다.
	constexpr double FrameStatsWindowSeconds = 0.5;
}

// 엔진 생성자에서도 사이클 카운터를 쓸 수 있게 가장 먼저 초기화한다.
FEngineLoop::FEngineLoop()
{
	FPlatformTime::InitTiming();
}

// TUniquePtr 멤버의 완전한 타입이 필요하므로 cpp에서 정의한다.
FEngineLoop::~FEngineLoop() = default;

bool FEngineLoop::PreInit(HINSTANCE hInstance, UClass* EngineClass)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	if (!EngineClass || !EngineClass->IsChildOf(UEngine::StaticClass()))
	{
		return false;
	}

	GEngine = NewObject<UEngine>(nullptr, EngineClass);
	if (!GEngine)
	{
		return false;
	}
	GEngine->EngineLoop = this;
	Config = GEngine->GetConfig();

	if (Config.SplashImage)
	{
		FSplashScreen::Show(hInstance, Config.SplashImage);
	}

	FSplashScreen::SetText(L"Initializing Renderer...");
	FSplashScreen::SetProgress(0.1f);

	// 0이면 주 모니터 해상도
	const uint32 Width = Config.Width != 0 ? Config.Width : static_cast<uint32>(GetSystemMetrics(SM_CXSCREEN));
	const uint32 Height = Config.Height != 0 ? Config.Height : static_cast<uint32>(GetSystemMetrics(SM_CYSCREEN));

	RenderDevice = MakeUnique<FRenderDevice>();
	RenderCommand::Init(RenderDevice.get());

	Renderer = MakeUnique<FRenderer>();
	Renderer->Init();

	MainWindow = MakeUnique<FWindow>();
	if (!MainWindow->Create(hInstance, Width, Height, Config.Title, Config.bBorderless))
	{
		FSplashScreen::Hide();
		return false;
	}

	Swapchain = MakeUnique<FSwapchain>(RenderDevice.get(), MainWindow.get());
	if (Config.bCreateDepthBuffer)
	{
		CreateDepthBuffer(MainWindow->GetWidth(), MainWindow->GetHeight());
	}

	FSplashScreen::SetText(L"Loading Assets...");
	FRenderResourceManager::Init();

	constexpr float AssetStart = 0.1f;
	constexpr float AssetEnd = 0.8f;
	UAssetManager::Init([](int32 Loaded, int32 Total, const FString& Path)
		{
			FSplashScreen::SetText("Loading " + Path);
			FSplashScreen::SetProgress(AssetStart + (AssetEnd - AssetStart) * Loaded / Total);
		});

	FSplashScreen::SetText(L"Initializing Engine...");
	FSplashScreen::SetProgress(0.8f);
	if (!GEngine->Init())
	{
		FSplashScreen::Hide();
		return false;
	}

	FSplashScreen::SetProgress(1.0f);
	FSplashScreen::Hide();

	// 초기화 동안 숨겨 두었던 메인 창을 스플래시가 닫힌 뒤에 띄운다.
	MainWindow->Show();

	EngineTimer::Init();
	PrevFrameCycles = FPlatformTime::GetCycles64();

	const uint32 Cores = std::max(1u, std::thread::hardware_concurrency());
	FTaskPool::Get().Init(std::min(Cores - 1, 15u));
	bIsRunning = true;
	return true;
}

bool FEngineLoop::Tick()
{
	EngineTimer::Tick();
	FInputSystem::UpdateInputStates();
	MainWindow->ProcessMessage(bIsRunning);

	if (Config.bExitOnEscape && FInputSystem::IsKeyPressed(EKeyCode::Escape))
	{
		RequestExit();
	}

	if (!bIsRunning)
	{
		return false;
	}

	HandleResize();

	// 최소화 중에는 그릴 대상이 없다.
	if (MainWindow->GetWidth() == 0 || MainWindow->GetHeight() == 0)
	{
		return true;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_EngineTick);
		GEngine->Tick(EngineTimer::GetDeltaTime());
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_Present);
		Swapchain->SwapBuffers(Config.SyncInterval);
	}

	// EngineTimer는 DeltaTime을 0.1초로 자르므로 표시용 시간은 사이클로 따로 잰다.
	const uint64 CurrentCycles = FPlatformTime::GetCycles64();
	UpdateFrameStats(CurrentCycles - PrevFrameCycles);
	PrevFrameCycles = CurrentCycles;

	return bIsRunning;
}

// UObject(에셋 포함)의 GPU 자원은 Device가 살아 있을 때 해제한다.
void FEngineLoop::Exit()
{
	FTaskPool::Get().Shutdown();


	if (GEngine)
	{
		GEngine->PreExit();
	}

	// UAssetManager도 UObject이므로 일괄 삭제 전에 종료한다.
	UAssetManager::Shutdown();
	FRenderResourceManager::Shutdown();

	DestroyAllObjectsExceptEngine();
	delete GEngine;
	GEngine = nullptr;

	DepthBuffer.reset();
	Swapchain.reset();
	Renderer.reset();

	if (RenderDevice)
	{
		RenderDevice->Shutdown();
	}
}

void FEngineLoop::BeginBackbufferPass()
{
	RenderCommand::BeginRenderPass(
		Swapchain->GetBackbuffer(),
		DepthBuffer.get(),
		MainWindow ? MainWindow->GetWidth() : 0,
		MainWindow ? MainWindow->GetHeight() : 0);

	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FEngineLoop::EndBackbufferPass()
{
	RenderCommand::EndRenderPass();
}

uint32 FEngineLoop::GetViewportWidth() const
{
	return MainWindow ? MainWindow->GetWidth() : 0;
}

uint32 FEngineLoop::GetViewportHeight() const
{
	return MainWindow ? MainWindow->GetHeight() : 0;
}

void FEngineLoop::HandleResize()
{
	if (!MainWindow->CheckResized())
	{
		return;
	}

	const uint32 Width = MainWindow->GetWidth();
	const uint32 Height = MainWindow->GetHeight();
	if (Width == 0 || Height == 0)
	{
		return;
	}

	Swapchain->Resize(Width, Height);
	if (Config.bCreateDepthBuffer)
	{
		CreateDepthBuffer(Width, Height);
	}
	GEngine->OnResize(Width, Height);
}

void FEngineLoop::CreateDepthBuffer(uint32 Width, uint32 Height)
{
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;   // 깊이 24bit + 스텐실 8bit
	Desc.SampleDesc.Count = 1;                     // 백버퍼와 동일해야 함 (MSAA 없음)
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;     // FTexture2D가 DSV를 만들어 준다

	DepthBuffer = RenderCommand::CreateTexture2D(Desc);
}

// 소멸자가 자신을 GUObjectArray에서 빼고 마지막 원소를 그 자리로 옮기므로 매번 끝에서 하나씩 지운다.
// 액터가 컴포넌트를 함께 지워도 배열 크기를 다시 읽으므로 안전하다.
void FEngineLoop::DestroyAllObjectsExceptEngine()
{
	while (true)
	{
		int32 Index = GUObjectArray.Num() - 1;
		if (Index >= 0 && GUObjectArray[Index] == GEngine)
		{
			--Index;
		}
		if (Index < 0)
		{
			break;
		}
		delete GUObjectArray[Index];
	}
}

void FEngineLoop::UpdateFrameStats(uint64 FrameCycles)
{
	FrameStats.LastFrameMs = FPlatformTime::ToMilliseconds64(FrameCycles);

	StatsWindowCycles += FrameCycles;
	++StatsWindowFrames;

	const double WindowSeconds = FPlatformTime::ToSeconds64(StatsWindowCycles);
	if (WindowSeconds < FrameStatsWindowSeconds)
	{
		return;
	}

	FrameStats.AverageFrameMs = WindowSeconds * 1000.0 / StatsWindowFrames;
	FrameStats.AverageFPS = StatsWindowFrames / WindowSeconds;
	StatsWindowCycles = 0;
	StatsWindowFrames = 0;
}
