#pragma once

#include "Engine/ViewportClient.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneViewTargets.h"

class UWorld;
class UGameInstance;
class UEngine;
class UCameraComponent;

struct FWorldContext;

// 게임 뷰포트 클라이언트 클래스
class FGameViewportClient : public FViewportClient
{
public:
	FGameViewportClient();
	virtual ~FGameViewportClient() override = default;

	// 월드 및 게임 인스턴스 초기화
	void Init(FWorldContext& InWorldContext, UEngine* InEngine);

	virtual UWorld* GetWorld() const override;

	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;
	bool ConsumeExitRequest() {
		const bool bRequested = bExitRequested;
		bExitRequested = false;
		return bRequested;
	}

	bool ConsumeSIERequest() {
		const bool bRequested = bSIEModeRequested;
		bSIEModeRequested = false;
		return bRequested;
	}

	// 게임 카메라 시점 계산
	FSceneView CalcSceneView(const FRect& InViewRect);

	// 렌더 타깃 크기 관리
	virtual void Resize(FViewport* Viewport, uint32 NewWidth, uint32 NewHeight) override { Resize(NewWidth, NewHeight); }
	void Resize(uint32 InWidth, uint32 InHeight);
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	class FTexture2D* GetColorTarget() const { return ViewTargets.SceneColor.get(); }
	FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output) { return ViewTargets.SwapSceneColorAndBind(Output); }
	class FTexture2D* GetDepthTarget() const { return ViewTargets.Depth.get(); }
	FSceneRenderData& GetRenderData() { return RenderData; }
	FSceneViewTargets& GetViewTargets() { return ViewTargets; }
	const FSceneViewTargets& GetViewTargets() const { return ViewTargets; }

	void SetGameInstance(UGameInstance* InGameInstance) { GameInstance = InGameInstance; }
	UGameInstance* GetGameInstance() const { return GameInstance; }

	void SetWorld(UWorld* InWorld);

	void SetCameraComponent(UCameraComponent* InCameraComponent);

	void Reset();

	bool BeginSIEMode();
	void EndSIEMode();
	void GetKeyInputBySIEMode(int32 Key, bool bDown);
	void GetAxisInputBySIEMode(EGameInputAxis Key, float Delta);
	void TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity);
	bool IsSIEMode() const { return bSIEMode; }

	virtual void LostFocus() override;

protected:
	virtual bool HandleUIKey(int32 Key, bool bDown) { return false; }
	virtual bool HandleUIAxis(EGameInputAxis AxisKey, float Delta) { return false; }
	virtual bool HandleUIMouseMove(int32 X, int32 Y) { return false; }

private:
	UCameraComponent* FindPlayerCamera() const;

	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
	UEngine* Engine = nullptr;
	UCameraComponent* CameraComponent = nullptr;
	bool bSIEMode = false;
	bool bPreviousMainCameraExternalInputManaged = false;
	bool bSIEKeyDown[256]{};
	float SIEMouseDeltaX = 0.0f;
	float SIEMouseDeltaY = 0.0f;
	float SIEWheelDelta = 0.0f;
	bool bExitRequested = false;
	bool bSIEModeRequested = false;

	// 렌더 타깃 버퍼
	uint32 Width = 0;
	uint32 Height = 0;
	FSceneViewTargets ViewTargets;
	FSceneRenderData RenderData;
};
