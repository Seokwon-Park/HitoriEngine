#pragma once

#include "Engine/Engine.h"
#include "Core/Window.h"
#include "Core/Types.h"

#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Render/RenderDevice.h"
#include "Render/Swapchain.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Editor/Gizmo/GizmoRenderer.h"
#include "Render/LineBatcher.h"
#include "Editor/Rendering/SceneDepthRenderer.h"
#include "Editor/Rendering/HeightFogRenderer.h"
#include "Editor/Rendering/FXAARenderer.h"
#include "Render/SceneRenderOptions.h"
#include "Render/SceneViewTargets.h"
#include "Render/RenderGraph/RenderGraph.h"


#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/LevelViewportLayout.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"

#include "Editor/Rendering/Outline.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "Render/SkyboxRenderer.h"

#include "PlayInEditorDataTypes.h"
#include "Core/Misc/Optional.h"

//Temp
#include "Text/Font.h"
#include "Text/TextRenderer.h"
#include <Engine\GameViewportClient.h>


inline constexpr int32 InvalidViewIndex = -1;

class UEditorEngine : public UEngine
{
	DECLARE_CLASS(UEditorEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;
	bool Init() override;
	void Tick(float DeltaTime) override;
	void PreExit() override;

	// Active View의 입력과 Picking 결과만 Gizmo 및 선택 상태에 반영한다.
	void UpdateGizmoAndPicking();
	// 뷰포트 하나의 씬과 에디터 요소를 렌더링한다.
	void RenderFrame(FEditorViewportClient* ViewClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const bool bIsPIE);
	// 게임 뷰포트 화면을 렌더링한다.
	void RenderGameFrame(FGameViewportClient* GameClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, bool bIsSIEMode = false);
	// 네 View 결과와 ImGui를 메인 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
	void PresentFrame();
	void DeleteActor(AActor* Actor);
	void DeleteComponent(UActorComponent* Component);

	//Play 버튼을 눌렀을때 Play Session 실행을 요청한다.
	inline void RequestPlaySession() { bPlaySessionRequested = true; }
	inline void RequestEndPlayMap() { bRequestEndPlayMapQueued = true; bIsPaused = false; }

	//실제 PIE 를 실행
	void StartPlayInEditorSession();
	UWorld* CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld);

	void OnActiveWorldChanged();

	void EndPlayMap();

	bool IsPlaying() const { return PlayWorld != nullptr; }
	EPIEState GetPIEState() const
	{
		if (!PlayWorld) return EPIEState::Stopped;
		if (bIsPaused) return EPIEState::Paused;
		return IsSimulatingInEditor() ? EPIEState::Simulating : EPIEState::Playing;
	}

	////FEditorDelegates::PrePIEEnded / EndPIE.Broadcast()
	//void TeardownPlaySession(const FWorldContext& PIEContext);            // 액터 EndPlay, 월드 정리(CleanupWorld)
	//void RestoreEditorWorld(UWorld* EditorWorld);            // GWorld를 에디터 월드로 복구
	//void DestroyWorldContext(UWorld* PlayWorld);

private:
	// 이번 프레임 DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
	void BeginFrame(float DeltaTime);
	// 패널 요청과 입력을 적용해 레이아웃·카메라 상태를 갱신한다.
	void UpdateViewportState(float DeltaTime);
	// 월드를 정확히 한 번 Tick·Capture한 뒤 에디터 상호작용을 갱신한다.
	void TickWorldAndEditor(float DeltaTime);
	// 한 번 캡처한 월드 결과를 재사용해 현재 레이아웃의 각 View를 렌더한다.
	void RenderViewports();
	// PIE View를 게임 카메라 상태와 동기화한다.
	void UpdatePIEViewCamera();
	void TickPIEInput(float DeltaTime);

	// 별도 PIE 창(New Editor Window)을 만들고 정리한다.
	bool CreatePIEWindow();
	void DestroyPIEWindow();
	// 창 크기에 맞춰 Swapchain과 오프스크린 타깃을 다시 만든다. 그릴 수 없으면 false다.
	bool UpdatePIEWindowTargets();
	// PIE 창의 오프스크린 결과를 백버퍼로 복사한다. Present는 EndFrame에서 한다.
	void CopyPIEWindowToBackbuffer();
	bool IsPIEInWindow() const { return PIEWindow != nullptr; }
	// 화면 합성과 View 설정 보관으로 프레임을 마무리한다.
	void EndFrame();

	bool RenderScene(FSceneViewTargets& Targets, const FSceneView& SceneView,
		FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const FSceneRenderOptions& Options,
		const std::function<void(FSceneOverlayCallbacks&)>& BuildOverlays);
	void RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView);
	bool RenderHeightFog(UWorld* TargetWorld, const FSceneView& SceneView, FTexture2D* DepthTarget, FTexture2D* ColorTarget);
	void RenderActorUUIDs(const FSceneView& View);

	FWorldContext& GetEditorWorldContext();
	FWorldContext* GetPIEWorldContext(int32 WorldPIEInstance = 0);
	UWorld* GetActiveWorld() const { return PlayWorld ? PlayWorld : EditorWorld; }

	// PIE 중이고 일시정지가 아닌 View는 Gizmo·Picking 등 에디터 상호작용을 하지 않는다.
	bool IsPlayingView(int32 ViewIndex) const { return PlayWorld && ViewIndex == PIEViewIndex && !bIsPaused && !IsSimulatingInEditor(); }
	bool IsSimulatingInEditor() const;

	// FEngineLoop 소유. OnInit에서 받아 둔다.
	FWindow* MainWindow = nullptr;
	FSwapchain* MainWindowSC = nullptr;
	FRenderer* Renderer = nullptr;

	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FLineBatcher> LineBatcher;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;
	TUniquePtr<FSceneDepthRenderer> SceneDepthRenderer;
	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FHeightFogRenderer> HeightFogRenderer;
	TUniquePtr<FFXAARenderer> FXAARenderer;
	TUniquePtr<FGizmo> Gizmo;

	UFont* SystemFont;

	FOutputLogPanel* OutputLogPanel = nullptr;

	FDetailsPanel* DetailsPanel = nullptr;
	FEditorControlsPanel* EditorControlsPanel = nullptr;
	FSettingsPanel* SettingsPanel = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	FSplitRatio ViewportSplitRatio{0.5f, 0.5f};
	ELayoutMode ViewportLayoutMode = ELayoutMode::QuadSplit;
	int32 SingleViewportIndex = 0;
	TArray<FEditorViewportClient*> AllViewportClients;
	FOutlinerPanel* OutlinerPanel = nullptr;
	FContentDrawerPanel* ContentDrawerPanel = nullptr;

	UWorld* EditorWorld = nullptr;
	UWorld* PlayWorld = nullptr;
	// PIE 시작 시점의 편집 View. PIE가 아니면 InvalidViewIndex다.
	int32 PIEViewIndex = -1;
	int32 PIEStartEditorViewIndex = -1;

	// Play 버튼이 다음 세션을 띄울 위치
	EPlayModeType PlayMode = EPlayModeType::InViewport;

	// New Editor Window(PIE) 모드에서만 존재한다. Swapchain이 창 핸들을 쓰므로 창보다 먼저 해제한다.
	TUniquePtr<FWindow> PIEWindow;
	TUniquePtr<FSwapchain> PIESwapchain;
	uint32 PIETargetWidth = 0;
	uint32 PIETargetHeight = 0;
	bool bPIEWindowRendered = false;

	FVector UUIDLocation;

	FEditorCommand Save;

	FEditorCommand Play;
	FEditorCommand Pause;
	FEditorCommand Resume;
	FEditorCommand StepFrame;
	FEditorCommand Stop;

	bool bIsStep = false;

	bool bPlaySessionRequested = false;
	bool bRequestEndPlayMapQueued = false;
	bool bStepRequested = false;
	bool bIsPaused = false;

	void ResetSceneSelection();

	void CreateNewScene();
	void OpenScene();
	void SaveCurrentScene();
	void SaveSceneAs();

	void CreatePIESession();
	void StopPIESession();
	void DrawPIEWindows();


	void UpdateInputOwner();
	void DispatchGameInput(float DeltaTime);
	FGameViewportClient* FindPIEGameClient(int32 PIEInstance) const;

	bool bIsSIEMode = false;

	int32 InputOwnerPIEInstance = -1;
	bool bGameMouseCaptured = false;
	int32 PendingFocusedPIEInstance = -1;
	int32 PendingWheelDelta = 0;

};
