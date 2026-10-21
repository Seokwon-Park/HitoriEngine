#pragma once

#include <Windows.h>

#include "Engine/Engine.h"

class UClass;
class FWindow;
class FRenderDevice;
class FRenderer;
class FSwapchain;
class FTexture2D;

// 프레임 시간 통계. FPS는 표시가 흔들리지 않게 일정 구간마다 평균을 낸다.
struct FFrameStats
{
	double LastFrameMs = 0.0;
	double AverageFrameMs = 0.0;
	double AverageFPS = 0.0;
};

// Device·Window·Swapchain과 메인 루프를 소유한다. 엔진 모드는 모르고 GEngine만 호출한다.
class FEngineLoop
{
public:
	FEngineLoop();
	~FEngineLoop();

	// EngineClass로 GEngine을 만들고 플랫폼 자원을 준비한다.
	bool PreInit(HINSTANCE hInstance, UClass* EngineClass);
	// 한 프레임을 진행한다. 종료 요청이 들어오면 false를 반환한다.
	bool Tick();
	void Exit();

	inline void RequestExit() { bIsRunning = false; }

	// 백버퍼(+깊이 버퍼)를 렌더 타깃으로 열고 불투명 3D 기본 상태를 설정한다.
	void BeginBackbufferPass();
	void EndBackbufferPass();

	FRenderDevice* GetRenderDevice() const { return RenderDevice.get(); }
	FRenderer* GetRenderer() const { return Renderer.get(); }
	FWindow* GetMainWindow() const { return MainWindow.get(); }
	FSwapchain* GetSwapchain() const { return Swapchain.get(); }
	// 백버퍼 패스의 깊이 버퍼. 기즈모처럼 장면 위에 항상 보여야 하는 것을 그리기 전에 지울 때 쓴다.
	FTexture2D* GetDepthBuffer() const { return DepthBuffer.get(); }

	uint32 GetViewportWidth() const;
	uint32 GetViewportHeight() const;
	const FFrameStats& GetFrameStats() const { return FrameStats; }

private:
	void HandleResize();
	void CreateDepthBuffer(uint32 Width, uint32 Height);
	void UpdateFrameStats(uint64 FrameCycles);
	// GEngine을 제외한 모든 UObject를 삭제한다. GEngine은 마지막에 따로 지운다.
	static void DestroyAllObjectsExceptEngine();

	// 선언 역순으로 해제되므로 의존 대상(Device)을 가장 위에 둔다.
	TUniquePtr<FRenderDevice> RenderDevice;
	TUniquePtr<FRenderer> Renderer;
	TUniquePtr<FWindow> MainWindow;
	TUniquePtr<FSwapchain> Swapchain;
	TUniquePtr<FTexture2D> DepthBuffer = nullptr;

	FEngineConfig Config;

	bool bIsRunning = false;

	FFrameStats FrameStats;
	uint64 PrevFrameCycles = 0;
	uint64 StatsWindowCycles = 0;
	uint32 StatsWindowFrames = 0;
};
