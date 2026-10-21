#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

#include "Containers/IndirectArray.h"

#include "Engine/World.h"
#include "EngineTypes.h"

#include "Asset/AssetManager.h"

class UGameInstance;
class FGameViewportClient;

struct FWorldContext
{
	EWorldType WorldType;
	FName ContextHandle;
	UGameInstance* OwningGameInstance = nullptr;
	int32 PIEInstance = 0;
	TSharedPtr<FGameViewportClient> GameViewport;

	void SetCurrentWorld(UWorld* World);

	inline UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	FWorldContext()
		: WorldType(EWorldType::None)
		, ContextHandle(NAME_None)
		, ThisCurrentWorld(nullptr)
	{
	}

private:
	UWorld* ThisCurrentWorld = nullptr;
};

class FEngineLoop;

// 엔진 모드별 실행 설정. UE에서는 ini가 맡는 부분이다.
// Width·Height가 0이면 주 모니터 해상도를 쓴다.
struct FEngineConfig
{
	const wchar_t* Title = L"Hitori";
	uint32 Width = 0;
	uint32 Height = 0;
	bool bBorderless = true;
	uint32 SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	bool bCreateDepthBuffer = true;   // 백버퍼에 3D를 직접 그리지 않는 엔진(에디터)은 끈다.
	bool bExitOnEscape = true;        // 테두리 없는 창에는 닫기 버튼이 없다.
	const char* SplashImage = nullptr;
};

// 모든 엔진 모드의 베이스. 플랫폼·루프·Present는 FEngineLoop가 맡고,
// 서브클래스(Game, Editor)는 Tick에서 한 프레임의 갱신과 렌더를 채운다.
class UEngine : public UObject
{
	DECLARE_CLASS(UEngine, UObject)
	friend class FEngineLoop;
public:
	// 창을 만들기 전에 호출된다.
	virtual FEngineConfig GetConfig() const { return {}; }

	// Device·Window·Swapchain·AssetManager가 준비된 뒤 호출된다.
	virtual bool Init();
	// 한 프레임의 갱신과 모든 렌더 패스를 기록한다. Present는 FEngineLoop가 호출 직후에 한다.
	virtual void Tick(float DeltaTime) = 0;
	virtual void OnResize(uint32 Width, uint32 Height) {}
	// UObject 일괄 정리와 GPU 자원 해제 전에 호출된다.
	virtual void PreExit() {};

	//UWorld* GetWorld() const { return World; }

	FWorldContext& CreateNewWorldContext(EWorldType WorldType);
	FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld);
	FWorldContext* GetWorldContextFromType(EWorldType WorldType);
	FWorldContext* GetWorldContextFromPIEInstance(int32 PIEInstance);
	UWorld* GetEditorWorld() const;
	virtual UWorld* CreatePIEWorldByDuplication(FWorldContext& Context, UWorld* SourceWorld);
	// PIE 컨텍스트는 게임 인스턴스와 복제 월드까지 함께 정리한다.
	void DestroyWorldContext(UWorld* InWorld);

protected:
	UAssetManager* AssetManager;

	FEngineLoop& GetEngineLoop() const { return *EngineLoop; }

	TIndirectArray<FWorldContext> WorldContextlist;
	//UWorld* World = nullptr;
	int32 NextWorldContextHandle = 0;
private:
	FEngineLoop* EngineLoop = nullptr;
};

// FEngineLoop::PreInit에서 설정되고 Exit에서 해제된다.
extern UEngine* GEngine;
