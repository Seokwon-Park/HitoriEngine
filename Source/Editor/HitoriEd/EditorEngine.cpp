#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"


#include "Render/GeometryGenerator.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"

#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor/LightActorBase.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "GameFramework/Actor/SpotLightActor.h"
#include "GameFramework/Actor/RectLightActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/TextRenderComponent.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "UObject/UObjectIterator.h"

#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"

namespace
{
	DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
	DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
	DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
	DECLARE_CYCLE_STAT("Capture World", STAT_CaptureWorld);
	DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
	DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);
}

#include "Core/SplashScreen.h"

FEngineConfig UEditorEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
{
	if (!Super::Init())
		return false;

	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);
	InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Editor, true));
	EditorWorld = InitialWorldContext.World();

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice* RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Gizmo = MakeUnique<FGizmo>();

	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	ViewportsPanel = EditorUI->AddEditorPanel<FViewportsPanel>();
	ContentDrawerPanel = EditorUI->AddEditorPanel<FContentDrawerPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SceneDepthRenderer = MakeUnique<FSceneDepthRenderer>();
	SceneDepthRenderer->Init(Renderer);

	HeightFogRenderer = MakeUnique<FHeightFogRenderer>();
	HeightFogRenderer->Init(Renderer);

	FXAARenderer = MakeUnique<FFXAARenderer>();
	FXAARenderer->Init(Renderer);

	SettingsPanel = EditorUI->AddEditorPanel<FSettingsPanel>();



	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/Pretendard.json");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	// 화면 나눔 비율 설정 가져오기
	ViewportSplitRatio = {
		SettingsPanel->GetSettings().MultipleViewportsHorizontal,
		SettingsPanel->GetSettings().MultipleViewportsVertical
	};
	// SingleView에 사용할 인덱스 설정
	SingleViewportIndex = SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex;
	// 뷰포트 레이아웃 설정
	ViewportLayoutMode = SettingsPanel->GetSettings().bMultipleViewportsSingle
		? ELayoutMode::Single
		: ELayoutMode::QuadSplit;
	EditorWorld->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);



	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(GetActiveWorld());
	OutlinerPanel->SetSelectionCallback(
		[this](USceneComponent* Root)
		{
			Gizmo->SetTarget(Root, true);
			DetailsPanel->SetTarget(Root ? Root->GetOwner() : nullptr);

			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Root);
			if (!Primitive && Root && Root->GetOwner())
			{
				for (UActorComponent* C : Root->GetOwner()->GetComponents())
					if ((Primitive = Cast<UPrimitiveComponent>(C))) break;
			}
			Outline->SetTarget(Primitive);
		});
	DetailsPanel->SetSelectionCallback(
		[this](UActorComponent* Component)
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				AActor* Owner = SceneComponent->GetOwner();
				if (Owner && SceneComponent == Owner->GetRootComponent())
				{
					Gizmo->SetTarget(SceneComponent, true);
				}
				else
				{
					Gizmo->SetTarget(SceneComponent);
				}
			}
			else
			{
				Gizmo->SetTarget(nullptr);
			}
		}
	);

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			DeleteActor(Actor);
		}
	);
	DetailsPanel->SetDeleteComponentCallback(
		[this](UActorComponent* Component)
		{
			DeleteComponent(Component);
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, GetActiveWorld());

	DetailsPanel->SetWorld(GetActiveWorld());

	EditorControlsPanel->SetWorld(EditorWorld);
	EditorControlsPanel->SetIsPlayingQuery([this]() { return IsPlaying(); });
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportsPanel(ViewportsPanel);

	SettingsPanel->SetWorld(EditorWorld);
	SettingsPanel->SetViewportsPanel(ViewportsPanel);
	ViewportsPanel->SetLayoutMode(ViewportLayoutMode, SingleViewportIndex);

	for (int32 i = 0; i < 4; ++i)
	{
		if (FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(i))
		{
			Client->SetWorld(EditorWorld);
			AllViewportClients.Add(Client);
		}
	}
	SettingsPanel->ApplyViewportSettings();

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init("Assets/SkySphere/Sky.hdr");

	const ImVec4 White = { 1, 1, 1, 1 };
	const ImVec4 Green = { 0.40f, 0.80f, 0.30f, 1.0f };

	Save = { "Save",   "Save Current Scene",UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Save.png"),  White, { EKeyCode::S, true } };

	Play = { "Play",   "Play World",        UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Play.png"),  Green, { EKeyCode::P, false, true } };

	Play.CanExecute = [this]()->bool {return !PlayWorld; };
	Play.Execute = [this]() { RequestPlaySession(); };

	Pause = { "Pause",  "Pause",			UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Pause.png"), White, { EKeyCode::Pause } };

	Pause.CanExecute = [this]()->bool {return !bIsPaused && PlayWorld; };
	Pause.Execute = [this]() { bIsPaused = true; if (PlayWorld) PlayWorld->SetPaused(true); };


	Resume = { "Resume", "Resume",          UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Play.png"),  White, { EKeyCode::Pause } };

	Resume.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	Resume.Execute = [this]() { bIsPaused = false; if (PlayWorld) PlayWorld->SetPaused(false); };

	StepFrame = { "Step", "Step One Frame", UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Step.png"),  White, {} };

	StepFrame.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	StepFrame.Execute = [this]() { bStepRequested= true; };


	Stop = { "Stop",   "Stop",				UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Toolbar/Stop.png"),  White, { EKeyCode::Escape } };

	Stop.CanExecute = [this]()->bool {return PlayWorld; };
	Stop.Execute = [this]() { RequestEndPlayMap(); };

	EditorUI->GetToolbar()->SetCommands({ Play, Pause, Resume, StepFrame, Stop });
	EditorUI->GetToolbar()->SetPlayModeBinding(
		[this]() { return PlayMode; },
		[this](EPlayModeType Mode) { PlayMode = Mode; },
		[this]() { return !IsPlaying(); });

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을 진행한다.
// 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에 한다.
void UEditorEngine::Tick(const float DeltaTime)
{
	// PIE 창 닫기 요청 처리
	if (PIEWindow && PIEWindow->ConsumeCloseRequest())
		RequestEndPlayMap();
	if (bRequestEndPlayMapQueued) { bRequestEndPlayMapQueued = false; EndPlayMap(); }
	if (bPlaySessionRequested) { bPlaySessionRequested = false; if (!PlayWorld) StartPlayInEditorSession(); }

	BeginFrame(DeltaTime);
	UpdateViewportState(DeltaTime);
	TickWorldAndEditor(DeltaTime);
	RenderViewports();
	EndFrame();
}

// DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
void UEditorEngine::BeginFrame(const float DeltaTime)
{
	FStatOverlay::Tick(DeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = DeltaTime;

	if (!ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
		DeleteActor(OutlinerPanel->GetSelectedActor());
}

// 패널 요청과 입력을 뷰포트 클라이언트에 전달해 레이아웃과 카메라 상태를 갱신한다.
void UEditorEngine::UpdateViewportState(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = SingleViewportIndex;

	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			SingleViewportIndex = RequestedSingleViewIndex;
		ViewportLayoutMode = RequestedLayout;

		FEditorSettings& Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		ViewportSplitRatio = ApplySplitterDrag(ViewportSplitRatio, EDragAxis::Horizontal, HorizontalDrag, ViewportSize, 0.1f);
	if (VerticalDrag != 0.0f)
		ViewportSplitRatio = ApplySplitterDrag(ViewportSplitRatio, EDragAxis::Vertical, VerticalDrag, ViewportSize, 0.1f);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal = ViewportSplitRatio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical = ViewportSplitRatio.Vertical;
	}

	FRect ViewRects[4]{};
	if (ViewportLayoutMode == ELayoutMode::Single)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			ViewRects[i] = (i == SingleViewportIndex)
				? FRect{ 0.0f, 0.0f, ViewportSize.X, ViewportSize.Y }
				: FRect{};
		}
	}
	else
	{
		ComputeViewRects(ViewportSplitRatio, ViewportSize, ViewRects);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		const bool bSlotActive = (ViewportLayoutMode == ELayoutMode::Single)
			? (i == SingleViewportIndex)
			: true;
		ViewportsPanel->SetView(i, ViewRects[i], bSlotActive);
	}

	// 마우스 호버 감지
	int32 HoveredViewIndex = -1;
	if (ViewportsPanel->IsHovered())
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (ViewRects[i].Width > 0.0f && ViewRects[i].Height > 0.0f &&
				LocalMousePosition.X >= ViewRects[i].X &&
				LocalMousePosition.X < ViewRects[i].X + ViewRects[i].Width &&
				LocalMousePosition.Y >= ViewRects[i].Y &&
				LocalMousePosition.Y < ViewRects[i].Y + ViewRects[i].Height)
			{
				HoveredViewIndex = i;
				break;
			}
		}
	}

	// 마우스 캡처 및 활성 뷰 결정
	static int32 CapturedViewportIndex = -1;
	if (FInputSystem::IsMouseReleased(EMouseButton::Right))
	{
		CapturedViewportIndex = -1;
	}
	if (FInputSystem::IsMousePressed(EMouseButton::Right) && HoveredViewIndex != -1)
	{
		CapturedViewportIndex = HoveredViewIndex;
		ImGui::SetWindowFocus("Viewports");
	}
	if (CapturedViewportIndex != -1)
	{
		ViewportsPanel->SetActiveViewIndex(CapturedViewportIndex);
	}
	else if (FInputSystem::IsMousePressed(EMouseButton::Left) && HoveredViewIndex != -1)
	{
		ViewportsPanel->SetActiveViewIndex(HoveredViewIndex);
	}

	const int32 WheelDelta = FInputSystem::GetWheelDelta();
	const float MoveSpeed = 10.0f * SettingsPanel->GetSettings().CameraSpeed;
	const float MouseSens = 0.2f * SettingsPanel->GetSettings().MouseSensitivity;

	for (int32 i = 0; i < 4; ++i)
	{
		FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(i);
		if (!Client || !Client->IsActive())
			continue;

		Client->TickInput(DeltaTime, i == CapturedViewportIndex,
			i == HoveredViewIndex, WheelDelta, MoveSpeed, MouseSens);
	}

	// 활성 뷰포트 시점 동기화
	const int32 ActiveIndex = ViewportsPanel->GetActiveViewIndex();
	if (FEditorViewportClient* ActiveClient = ViewportsPanel->GetViewportClient(ActiveIndex))
	{
		if (ACameraActor* MainCam = GetActiveWorld()->GetMainCamera())
		{
			if (UCameraComponent* CamCom = MainCam->GetCameraComponent())
			{
				CamCom->SetRelativeLocation(ActiveClient->GetViewLocation());
				CamCom->SetRelativeRotation(ActiveClient->GetViewRotation());
				CamCom->SetFieldOfView(ActiveClient->GetViewFOV());
				CamCom->SetOrthoWidth(ActiveClient->GetOrthoWidth());
				CamCom->SetIsOrthogonal(!ActiveClient->IsPerspective());
			}
		}
	}
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorldAndEditor(const float DeltaTime)
{
	// 게임 입력 처리
	if (PlayWorld)
	{
		TickPIEInput(DeltaTime);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		if (bStepRequested && bIsPaused)
		{
			GetActiveWorld()->SetPaused(false);
			GetActiveWorld()->Tick(1.0f / 60.0f);
			GetActiveWorld()->SetPaused(true);
			bStepRequested = false;
		}
		else if(!bIsPaused)
			GetActiveWorld()->Tick(DeltaTime);

	}
	{
		SCOPE_CYCLE_COUNTER(STAT_EditorTick);
		EditorUI->Tick(DeltaTime);
	}
	UpdateGizmoAndPicking();
	EditorWorld->GetScene().UpdateAllTransforms();
	if (PlayWorld && bIsPaused)
		PlayWorld->GetScene().UpdateAllTransforms();
}

// 활성 View별 렌더를 수행한다.
void UEditorEngine::RenderViewports()
{
	const bool bIsPIE = (PlayWorld != nullptr);

	for (int32 ViewIndex = 0; ViewIndex < AllViewportClients.Num(); ++ViewIndex) {
		FEditorViewportClient* ViewClient = AllViewportClients[ViewIndex];
		if (!ViewClient || !ViewClient->IsActive())
			continue;

		const FRect& ViewRect = ViewClient->GetRect();
		if (ViewRect.Width <= 0.0f || ViewRect.Height <= 0.0f)
			continue;

		bool bIsGameView = false;
		int32 TargetPlayerIndex = -1;

		if (bIsPIE && !IsPIEInWindow() && PlayMode == EPlayModeType::InViewport &&
			ViewIndex == PIEStartEditorViewIndex)
		{
			bIsGameView = true;
			TargetPlayerIndex = 0;
		}

		// 선택된 게임 뷰포트와 나머지 에디터 뷰포트는 같은 PIE 월드를 본다.
		UWorld* CurrentWorld = bIsPIE ? PlayWorld : EditorWorld;
		FSceneView SceneView;

		if (bIsGameView && TargetPlayerIndex >= 0)
		{
			FGameViewportClient* GameClient = nullptr;
			for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx)
			{
				if (WorldContextlist[WorldIdx].WorldType == EWorldType::PIE &&
					WorldContextlist[WorldIdx].PIEInstance == TargetPlayerIndex &&
					WorldContextlist[WorldIdx].GameViewport)
				{
					GameClient = WorldContextlist[WorldIdx].GameViewport.get();
					if (WorldContextlist[WorldIdx].World())
					{
						CurrentWorld = WorldContextlist[WorldIdx].World();
					}

					SceneView = GameClient->CalcSceneView(ViewRect);

					FSceneRenderer SceneRenderer(CurrentWorld, SceneView, GameClient->GetRenderData());
					SceneRenderer.InitViews(Renderer);


					RenderGameFrame(GameClient, SceneView, SceneRenderer, bIsSIEMode);

					break;
				}
			}
		}
		else
		{
			SceneView = ViewClient->CalcSceneView(ViewRect);

			FSceneRenderer SceneRenderer(CurrentWorld, SceneView, ViewClient->GetRenderData());
			SceneRenderer.InitViews(Renderer);

			RenderFrame(ViewClient, SceneView, SceneRenderer, CurrentWorld, bIsGameView);
		}
	}

	// 별도 창(FWindow) 모드인 경우 PIE 창 타깃에 렌더링하고 스왑체인 백버퍼로 복사한다.
	bPIEWindowRendered = false;
	if (bIsPIE && IsPIEInWindow() && UpdatePIEWindowTargets())
	{
		FWorldContext* Context = GetPIEWorldContext();
		if (Context && Context->GameViewport && Context->World())
		{
			FGameViewportClient* GameClient = Context->GameViewport.get();
			const uint32 Width = GameClient->GetWidth();
			const uint32 Height = GameClient->GetHeight();
			if (Width > 0 && Height > 0)
			{
				FRect ViewRect{ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) };
				FSceneView SceneView = GameClient->CalcSceneView(ViewRect);
				FSceneRenderer SceneRenderer(Context->World(), SceneView, GameClient->GetRenderData());
				SceneRenderer.InitViews(Renderer);
				RenderGameFrame(GameClient, SceneView, SceneRenderer, bIsSIEMode);
				CopyPIEWindowToBackbuffer();
			}
		}
	}
}

void UEditorEngine::UpdatePIEViewCamera()
{
	if (!PlayWorld || PIEViewIndex == InvalidViewIndex)
		return;
	FWorldContext* Context = GetPIEWorldContext();
	if (!Context || !Context->GameViewport)
		return;
	FGameViewportClient& GameViewport = *Context->GameViewport;
	if (PIEViewIndex >= 0 && PIEViewIndex < AllViewportClients.Num())
	{
		if (FEditorViewportClient* View = AllViewportClients[PIEViewIndex])
		{
			const FRect& GameRect = View->GetRect();
			if (GameRect.Width <= 0.0f || GameRect.Height <= 0.0f) return;
			const FSceneView GameView = GameViewport.CalcSceneView(GameRect);
			View->SetViewLocation(GameView.ViewLocation);
			View->SetViewRotation(GameView.ViewRotation);
			View->SetViewFOV(GameView.FieldOfView);
			View->SetOrthoWidth(GameView.OrthoWidth);
			View->SetIsPerspective(GameView.bIsPerspective);
		}
	}
}

bool UEditorEngine::IsSimulatingInEditor() const
{
	for (const FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == EWorldType::PIE && Context.GameViewport)
			return Context.GameViewport->IsSIEMode();
	return false;
}

void UEditorEngine::TickPIEInput(float DeltaTime)
{
	FWorldContext* Context = GetPIEWorldContext();
	if (!Context || !Context->GameViewport) return;
	FGameViewportClient& GameViewport = *Context->GameViewport;
	const FVector2 Mouse = ViewportsPanel->GetLocalMousePosition();
	const int32 InputViewIndex = IsPIEInWindow() && GameViewport.IsSIEMode()
		? PIEStartEditorViewIndex : PIEViewIndex;
	const FRect PIEViewRect = (InputViewIndex >= 0 && InputViewIndex < AllViewportClients.Num() && AllViewportClients[InputViewIndex])
		? AllViewportClients[InputViewIndex]->GetRect() : FRect{};
	const bool bMouseOverPIE = Mouse.X >= PIEViewRect.X && Mouse.Y >= PIEViewRect.Y &&
		Mouse.X < PIEViewRect.X + PIEViewRect.Width && Mouse.Y < PIEViewRect.Y + PIEViewRect.Height;
	const int32 ActiveViewIdx = ViewportsPanel ? ViewportsPanel->GetActiveViewIndex() : -1;
	const bool bEditorSIEFocus = IsPIEInWindow() && GameViewport.IsSIEMode() &&
		::GetForegroundWindow() == MainWindow->GetHandle() &&
		ActiveViewIdx == PIEStartEditorViewIndex && ViewportsPanel->IsHovered() &&
		!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
	const bool bFocused = IsPIEInWindow()
		? ::GetForegroundWindow() == PIEWindow->GetHandle() || bEditorSIEFocus
		: ::GetForegroundWindow() == MainWindow->GetHandle() &&
			ActiveViewIdx == InputViewIndex &&
			ViewportsPanel->IsHovered() &&
			!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
	if (!bFocused) { GameViewport.LostFocus(); return; }
	if (!IsPIEInWindow() &&
		(FInputSystem::IsMousePressed(EMouseButton::Left) || FInputSystem::IsMousePressed(EMouseButton::Right)) &&
		!bMouseOverPIE)
	{
		GameViewport.LostFocus();
		return;
	}

	if (FInputSystem::IsKeyPressed(EKeyCode::F8))
		GameViewport.InputKey(nullptr, static_cast<int32>(EKeyCode::F8), true);
	if (GameViewport.ConsumeSIERequest())
	{
		if (GameViewport.IsSIEMode()) GameViewport.EndSIEMode();
		else GameViewport.BeginSIEMode();
		OnActiveWorldChanged();
	}
	if (FInputSystem::IsKeyPressed(EKeyCode::Escape))
		GameViewport.InputKey(nullptr, static_cast<int32>(EKeyCode::Escape), true);
	if (GameViewport.ConsumeExitRequest())
	{
		RequestEndPlayMap();
		return;
	}
	constexpr EKeyCode Keys[] = { EKeyCode::W, EKeyCode::A, EKeyCode::S, EKeyCode::D,
		EKeyCode::Space, EKeyCode::Shift };
	for (EKeyCode Key : Keys)
	{
		if (FInputSystem::IsKeyPressed(Key))
		{
			if (GameViewport.IsSIEMode()) GameViewport.GetKeyInputBySIEMode(static_cast<int32>(Key), true);
			else GameViewport.InputKey(nullptr, static_cast<int32>(Key), true);
		}
		if (FInputSystem::IsKeyReleased(Key))
		{
			if (GameViewport.IsSIEMode()) GameViewport.GetKeyInputBySIEMode(static_cast<int32>(Key), false);
			else GameViewport.InputKey(nullptr, static_cast<int32>(Key), false);
		}
	}
	constexpr EMouseButton MouseButtons[] = { EMouseButton::Left, EMouseButton::Right };
	for (EMouseButton Button : MouseButtons)
	{
		const int32 Key = Button == EMouseButton::Left ? static_cast<int32>(EKeyCode::LButton) : static_cast<int32>(EKeyCode::RButton);
		if (FInputSystem::IsMousePressed(Button))
		{
			if (GameViewport.IsSIEMode()) GameViewport.GetKeyInputBySIEMode(Key, true);
			else GameViewport.InputKey(nullptr, Key, true);
		}
		if (FInputSystem::IsMouseReleased(Button))
		{
			if (GameViewport.IsSIEMode()) GameViewport.GetKeyInputBySIEMode(Key, false);
			else GameViewport.InputKey(nullptr, Key, false);
		}
	}
	const float MouseX = static_cast<float>(FInputSystem::GetMouseDeltaX());
	const float MouseY = static_cast<float>(FInputSystem::GetMouseDeltaY());
	if (GameViewport.IsSIEMode())
	{
		GameViewport.GetAxisInputBySIEMode(EGameInputAxis::MouseX, MouseX);
		GameViewport.GetAxisInputBySIEMode(EGameInputAxis::MouseY, MouseY);
		GameViewport.TickSIEInput(DeltaTime, SettingsPanel->GetSettings().CameraSpeed,
			SettingsPanel->GetSettings().MouseSensitivity);
	}
	else
	{
		GameViewport.InputAxis(nullptr, 0, EGameInputAxis::MouseX, MouseX, DeltaTime);
		GameViewport.InputAxis(nullptr, 0, EGameInputAxis::MouseY, MouseY, DeltaTime);
	}
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame()
{
	PresentFrame();
	// 메인 Swapchain은 FEngineLoop가 VSync 설정대로 Present하므로 PIE 창은 기다리지 않고 올린다.
	if (bPIEWindowRendered && PIESwapchain)
		PIESwapchain->SwapBuffers(0);
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

FWorldContext& UEditorEngine::GetEditorWorldContext()
{
	for (int32 i = 0; i < WorldContextlist.Num(); ++i)
	{
		if (WorldContextlist[i].WorldType == EWorldType::Editor)
		{
			return WorldContextlist[i];
		}
	}

	return CreateNewWorldContext(EWorldType::Editor);
}

FWorldContext* UEditorEngine::GetPIEWorldContext(int32 WorldPIEInstance)
{
	for (FWorldContext& WorldContext : WorldContextlist)
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.PIEInstance == WorldPIEInstance)
		{
			return &WorldContext;
		}
	}

	return nullptr;
}



void UEditorEngine::UpdateGizmoAndPicking()
{
	const int32 ViewIndex = ViewportsPanel ? ViewportsPanel->GetActiveViewIndex() : -1;
	if (ViewIndex < 0 || ViewIndex >= 4 || !ViewportsPanel || !ViewportsPanel->IsHovered())
		return;
	if (IsPlayingView(ViewIndex))
		return;

	FEditorViewportClient* ViewClient = ViewportsPanel->GetViewportClient(ViewIndex);
	if (!ViewClient || !ViewClient->IsActive())
		return;

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	ViewClient->UpdateGizmoAndPicking(Gizmo.get(), LocalMousePosition, [this](AActor* SelectedActor) {
		OutlinerPanel->SelectActor(SelectedActor);
	});
}

// 게임 뷰포트 화면을 렌더링한다
void UEditorEngine::RenderGameFrame(FGameViewportClient* GameClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, bool bIsSIEMode)
{
	if (!GameClient) return;

	FSceneRenderOptions Options;
	Options.bEnableFXAA = SettingsPanel && SettingsPanel->GetSettings().bEnableFXAA;
	if (SettingsPanel)
	{
		Options.RenderingPath = SettingsPanel->GetSettings().RenderingPath;
		Options.LightingModel = SettingsPanel->GetSettings().LightingModel;
		Options.LightCullingMode = SettingsPanel->GetSettings().LightCullingMode;
	}

	FSceneViewTargets& Targets = GameClient->GetViewTargets();
	const bool bRendered = RenderScene(Targets, SceneView, SceneRenderer, GameClient->GetWorld(), Options,
		[this, &Targets, &SceneView, bIsSIEMode](FSceneOverlayCallbacks& Overlays)
	{
		if (!bIsSIEMode) return;
		const FViewportSettings ViewportSetting{ 0, 0, Targets.SceneColor->GetWidth(),
			Targets.SceneColor->GetHeight(), 0.0f, 1.0f };
		if (Outline->GetTarget())
			Overlays.SIEOutline = [this, &SceneView, ViewportSetting]
				{
					OutlineRenderer->OnRender(*Outline, SceneView.ViewProjectionMatrix, ViewportSetting);
				};
		if (Gizmo->GetTarget())
			Overlays.SIEGizmo = [this, &SceneView]
				{
					GizmoRenderer->OnRender(*Gizmo, SceneView.ViewProjectionMatrix,
						SceneView.ViewLocation, !SceneView.bIsPerspective);
				};
	});
	if (!bRendered) return;
}

bool UEditorEngine::RenderScene(FSceneViewTargets& Targets, const FSceneView& SceneView,
	FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const FSceneRenderOptions& Options,
	const std::function<void(FSceneOverlayCallbacks&)>& BuildOverlays)
{
	FTexture2D* ColorTarget = Targets.SceneColor.get();
	FTexture2D* DepthTarget = Targets.Depth.get();
	if (!TargetWorld || !Renderer || !ColorTarget || !DepthTarget)
		return false;
	const uint32 Width = ColorTarget->GetWidth();
	const uint32 Height = ColorTarget->GetHeight();
	if (!Width || !Height || !Targets.IsValidFor(Width, Height))
		return false;

	FScenePassCallbacks Callbacks;
	Callbacks.HeightFog = [this, TargetWorld, &SceneView, DepthTarget, ColorTarget]
	{
		return RenderHeightFog(TargetWorld, SceneView, DepthTarget, ColorTarget);
	};
	Callbacks.TranslucentFogConstants = [this](bool bHasFog)
	{
		return HeightFogRenderer ? HeightFogRenderer->GetTranslucentFogConstants(bHasFog) : nullptr;
	};
	Callbacks.SceneText = [this, TargetWorld, &SceneView] { RenderSceneText(TargetWorld, SceneView); };
	if (SceneDepthRenderer)
		Callbacks.DepthVisualization = [this, &SceneView, DepthTarget, ColorTarget, Range = Options.SceneDepthRange]
		{
			SceneDepthRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Range);
		};

	const bool bUseFXAA = Options.bEnableFXAA && !Options.bShowSceneDepth &&
		!Options.bShowWorldNormal && FXAARenderer && FXAARenderer->PrepareOutput(ColorTarget);
	if (bUseFXAA)
	{
		Callbacks.FXAAOutput = FXAARenderer->GetOutputTarget().get();
		Callbacks.FXAA = [this, ColorTarget] { return FXAARenderer->OnRender(ColorTarget); };
	}

	if (BuildOverlays) BuildOverlays(Callbacks.Overlays);

	FRenderGraphBuilder Graph;
	if (!SceneRenderer.AddViewportPasses(Graph, Renderer, Targets, Options, SkyboxRenderer.get(), Callbacks) ||
		!Graph.Execute() || !SceneRenderer.DidCorePassSucceed() || !SceneRenderer.DidFXAAPassSucceed())
		return false;
	if (bUseFXAA && !Targets.SwapSceneColorAndBind(FXAARenderer->GetOutputTarget()))
		return false;

	RenderCommand::EndRenderPass();
	return true;
}

void UEditorEngine::RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView)
{
	if (!TextRenderer) return;
	for (TObjectIterator<UTextRenderComponent> Text; Text; ++Text)
	{
		if (!Text || !Text->IsRegistered() || !Text->GetFont() || !Text->IsShown(SceneView.bGameView) || !Text->GetOwner() ||
			Text->GetOwner()->GetWorld() != TargetWorld)
			continue;
		TextRenderer->OnRender(Text->GetText(), Text->GetWorldMatrix(), Text->GetTextSize(),
			*Text->GetFont(), SceneView.ViewProjectionMatrix);
	}
}

bool UEditorEngine::RenderHeightFog(UWorld* TargetWorld, const FSceneView& SceneView,
	FTexture2D* DepthTarget, FTexture2D* ColorTarget)
{
	if (!TargetWorld || !HeightFogRenderer || !DepthTarget || !ColorTarget ||
		!SceneView.bIsPerspective)
		return false;

	for (TObjectIterator<UExponentialHeightFogComponent> Fog; Fog; ++Fog)
	{
		if (!Fog || !Fog->IsRegistered() || !Fog->IsShown(SceneView.bGameView) || !Fog->GetOwner() ||
			Fog->GetOwner()->GetWorld() != TargetWorld)
			continue;

		FHeightFogSetting Setting;
		Setting.FogDensity = Fog->GetFogDensity();
		Setting.FogHeightFalloff = Fog->GetFogHeightFalloff();
		Setting.FogHeight = Fog->GetFogHeight();
		Setting.FogColor = Fog->GetFogColor();
		Setting.StartDistance = Fog->GetStartDistance();
		Setting.FogMaxOpacity = Fog->GetFogMaxOpacity();
		Setting.FogCutoffDistance = Fog->GetFogCutoffDistance();
		if (HeightFogRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Setting))
			return true;
	}
	return false;
}

void UEditorEngine::RenderActorUUIDs(const FSceneView& SceneView)
{
	const bool bOrtho = !SceneView.bIsPerspective;
	UWorld* TargetWorld = PlayWorld ? PlayWorld : EditorWorld;

	for (AActor* Actor : TargetWorld->GetPersistentLevel()->GetActors())
	{
		if (!Actor)
			continue;
		USceneComponent* Root = Actor->GetRootComponent();

		if (!Root)
			continue;

		FBox Box = Root->CalcBounds();
		FVector UUIDLoc((Box.Min.X + Box.Max.X) * 0.5f,
			(Box.Min.Y + Box.Max.Y) * 0.5f, Box.Max.Z + 0.5f);

		FString Text = "UUID : " + std::to_string(Actor->GetUUID());
		TextRenderer->BuildTextMesh(Text, 0.5f, *SystemFont);

		FVector Facing = bOrtho ? (SceneView.ViewForward * -1.0f)
			: (SceneView.ViewLocation - UUIDLoc).Normalized();
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		FVector Right = FVector::Cross(Up, Facing).Normalized();
		FVector RealUp = FVector::Cross(Facing, Right).Normalized();
		FMatrix BillboardWorld = FMatrix::Identity;
		BillboardWorld.M[0][0] = Facing.X;
		BillboardWorld.M[0][1] = Facing.Y;
		BillboardWorld.M[0][2] = Facing.Z;
		BillboardWorld.M[1][0] = Right.X;
		BillboardWorld.M[1][1] = Right.Y;
		BillboardWorld.M[1][2] = Right.Z;
		BillboardWorld.M[2][0] = RealUp.X;
		BillboardWorld.M[2][1] = RealUp.Y;
		BillboardWorld.M[2][2] = RealUp.Z;
		BillboardWorld.M[3][0] = UUIDLoc.X;
		BillboardWorld.M[3][1] = UUIDLoc.Y;
		BillboardWorld.M[3][2] = UUIDLoc.Z;

		TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont,
			SceneView.ViewProjectionMatrix);
	}
}

// 뷰포트 하나의 씬과 에디터 요소를 렌더링한다
void UEditorEngine::RenderFrame(FEditorViewportClient* ViewClient,
	const FSceneView& SceneView,
	FSceneRenderer& SceneRenderer,
	UWorld* TargetWorld,
	const bool bIsPIE)
{
	if (!ViewClient || !SettingsPanel) return;
	FSceneRenderOptions Options;
	Options.bDrawPrimitives = bIsPIE || SettingsPanel->GetSettings().bDrawPrimitives;
	Options.bEnableFog = ViewClient->GetViewportMode() == EViewportMode::Solid;
	Options.bEnableFXAA = SettingsPanel->GetSettings().bEnableFXAA;
	Options.RenderingPath = SettingsPanel->GetSettings().RenderingPath;
	Options.LightingModel = SettingsPanel->GetSettings().LightingModel;
	Options.LightCullingMode = SettingsPanel->GetSettings().LightCullingMode;
	Options.bShowSceneDepth = !bIsPIE && ViewClient->IsSceneDepth();
	Options.bShowWorldNormal = !bIsPIE && ViewClient->IsWorldNormal();
	Options.bShowLightCulling = !bIsPIE && ViewClient->IsLightCulling();
	Options.SceneDepthRange = ViewClient->GetMaxRange();

	FSceneViewTargets& Targets = ViewClient->GetViewTargets();
	const bool bRendered = RenderScene(Targets, SceneView, SceneRenderer, TargetWorld, Options,
		[this, ViewClient, &SceneView, &Targets, bIsPIE](FSceneOverlayCallbacks& Overlays)
	{
		if (bIsPIE) return;
		const FViewportSettings ViewportSetting{ 0, 0, Targets.SceneColor->GetWidth(),Targets.SceneColor->GetHeight(), 0.0f, 1.0f };
		const bool bIsActiveViewport = ViewportsPanel && SceneView.ViewIndex == ViewportsPanel->GetActiveViewIndex();
		
		const bool bDrawSelectedLight = bIsActiveViewport && Gizmo->GetTarget() &&
			(Cast<ASpotLightActor>(Gizmo->GetTarget()->GetOwner()) ||
			 Cast<APointLightActor>(Gizmo->GetTarget()->GetOwner()) ||
			 Cast<ARectLightActor>(Gizmo->GetTarget()->GetOwner()));

		if (SettingsPanel->GetSettings().bDrawBatchLine || bDrawSelectedLight)
		{
			Overlays.EditorLines = [this, &SceneView, bDrawSelectedLight]
				{
					LineBatcher->BeginFrame();
					if (SettingsPanel->GetSettings().bDrawBatchLine && SettingsPanel->GetSettings().bDrawBoundingBox)
					{
						LineBatcher->BuildVertexBuffer();
						EditorWorld->GetPathTracker().OnRender(LineBatcher.get());
					}
					if (bDrawSelectedLight)
					{
						if (ASpotLightActor* Light = Cast<ASpotLightActor>(Gizmo->GetTarget()->GetOwner()))
							Light->GetSpotLightComponent()->DrawDebug(LineBatcher.get());
						else if (APointLightActor* Light = Cast<APointLightActor>(Gizmo->GetTarget()->GetOwner()))
							Light->GetPointLightComponent()->DrawDebug(LineBatcher.get());
						else if (ARectLightActor* Light = Cast<ARectLightActor>(Gizmo->GetTarget()->GetOwner()))
							Light->GetRectLightComponent()->DrawDebug(LineBatcher.get());
					}
					LineBatcher->OnRender(SceneView.ViewProjectionMatrix);
				};
		}
		if (SettingsPanel->GetSettings().bDrawBatchLine)
		{
			Overlays.EditorGrid = [this, ViewClient, &SceneView, ViewportSetting]
				{
					EGridPlane GridPlane = EGridPlane::XY;
					switch (ViewClient->GetViewportType())
					{
					case ELevelViewportType::Front: GridPlane = EGridPlane::YZ; break;
					case ELevelViewportType::Right: GridPlane = EGridPlane::XZ; break;
					default: break;
					}
					if (SettingsPanel->GetSettings().bDrawPSGrid && SceneView.bIsPerspective)
						GridRenderer->OnRenderPSGrid(SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
							SettingsPanel->GetSettings(), ViewportSetting);
					else
						GridRenderer->OnRenderBatchGrid(SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
							SceneView.ViewForward, GridPlane,
							static_cast<float>(SettingsPanel->GetSettings().GridSpacing),
							SceneView.bIsPerspective, ViewportSetting);
				};
		}
		if (bIsActiveViewport && Outline->GetTarget())
			Overlays.EditorOutline = [this, &SceneView, ViewportSetting]
				{
					OutlineRenderer->OnRender(*Outline, SceneView.ViewProjectionMatrix, ViewportSetting);
				};
		if (bIsActiveViewport && Gizmo->GetTarget())
			Overlays.EditorGizmo = [this, &SceneView]
				{
					GizmoRenderer->OnRender(*Gizmo, SceneView.ViewProjectionMatrix,
						SceneView.ViewLocation, !SceneView.bIsPerspective);
				};
		Overlays.bClearEditorDepth = true;
		if (SettingsPanel->GetSettings().bShowUUID)
			Overlays.EditorUUID = [this, &SceneView] { RenderActorUUIDs(SceneView); };
	});
	if (!bRendered) return;
}

// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
void UEditorEngine::PresentFrame()
{
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetBackbuffer(), nullptr,
		MainWindow ? MainWindow->GetWidth() : 0,
		MainWindow ? MainWindow->GetHeight() : 0);

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass();
}

void UEditorEngine::DrawPIEWindows()
{
}

// ImGui를 정리한다. UObject 일괄 삭제와 공용 자원·Device 정리는 FEngineLoop가 이어서 한다.
void UEditorEngine::PreExit()
{
	EndPlayMap();
	EditorWorld->ClearWorld();
	// PIE 창이 열린 채 종료하면 Device가 살아 있을 때 Swapchain과 타깃을 먼저 정리한다.
	DestroyPIEWindow();
	ImGuiRenderer->Shutdown();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;
	// 실행 중 삭제는 게임 로직이 들고 있는 참조를 깨뜨릴 수 있어 Pause 상태에서만 허용한다.
	if (IsPlaying() && !bIsPaused && !IsSimulatingInEditor())
		return;

	OutlinerPanel->SelectActor(nullptr);
	DetailsPanel->SelectComponent(nullptr);

	Actor->Destroy();
}

void UEditorEngine::DeleteComponent(UActorComponent* Component)
{
	if (!Component)
		return;
	if (IsPlaying() && !bIsPaused && !IsSimulatingInEditor())
		return;
	DetailsPanel->SelectComponent(nullptr);
	Outline->SetTarget(nullptr);
	Component->GetOwner()->DestroyComponent(Component);
}

void UEditorEngine::StartPlayInEditorSession()
{
	// 별도 창 모드면 패널 View는 모두 에디터 월드로 두고 창 하나에만 PIE 월드를 그린다.
	// 창을 만들지 못하면 선택한 View에서 실행한다.
	const bool bNewWindow = (PlayMode == EPlayModeType::InEditorFloating);

	PIEStartEditorViewIndex = ViewportsPanel ? ViewportsPanel->GetActiveViewIndex() : 0;
	if (bNewWindow && CreatePIEWindow())
		PIEViewIndex = -2;
	else
		// 지금 편집 중인 View 하나만 PIE 월드를 보여준다.
		PIEViewIndex = PIEStartEditorViewIndex;

	FWorldContext& PIEContext = CreateNewWorldContext(EWorldType::PIE);
	PIEContext.PIEInstance = 0;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	GameInstance->SetWorldContext(&PIEContext);
	if (!GameInstance->InitializeForPlayInEditor(0) ||
		!GameInstance->StartPlayInEditorGameInstance())
	{
		UWorld* FailedWorld = PIEContext.World();
		DestroyWorldContext(FailedWorld);
		PIEViewIndex = InvalidViewIndex;
		PIEStartEditorViewIndex = InvalidViewIndex;
		DestroyPIEWindow();
		return;
	}
	PlayWorld = GameInstance->GetWorld();
	if (!bNewWindow && PIEContext.GameViewport && ViewportsPanel)
	{
		ViewportsPanel->SetGameViewportClient(PIEStartEditorViewIndex, PIEContext.GameViewport.get());
	}

	OnActiveWorldChanged();
}

UWorld* UEditorEngine::CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld)
{
	UWorld* NewPIEWorld = UWorld::GetDuplicatedWorldForPIE(InEditorWorld);
	if (!NewPIEWorld) return nullptr;
	const int32 StartView = PIEStartEditorViewIndex;
	if (StartView >= 0 && StartView < AllViewportClients.Num() && AllViewportClients[StartView] && NewPIEWorld->GetMainCamera())
	{
		FEditorViewportClient* Source = AllViewportClients[StartView];
		UCameraComponent* Camera = NewPIEWorld->GetMainCamera()->GetCameraComponent();
		Camera->SetWorldLocationAndRotation(Source->GetViewLocation(), Source->GetViewRotation(), false);
		Camera->SetFieldOfView(Source->GetViewFOV());
		Camera->SetOrthoWidth(Source->GetOrthoWidth());
		Camera->SetIsOrthogonal(!Source->IsPerspective());
	}
	PIEContext.SetCurrentWorld(NewPIEWorld);
	return NewPIEWorld;
}

void UEditorEngine::OnActiveWorldChanged()
{
	ResetSceneSelection();
	UWorld* UIWorld = PlayWorld ? PlayWorld : EditorWorld;
	if (OutlinerPanel) OutlinerPanel->SetWorld(UIWorld);
	if (DetailsPanel) DetailsPanel->SetWorld(UIWorld);
	if (EditorControlsPanel) EditorControlsPanel->SetWorld(UIWorld);
	if (SettingsPanel) SettingsPanel->SetWorld(UIWorld);
}

void UEditorEngine::EndPlayMap()
{
	if (!PlayWorld)
		return;
	bIsPaused = false;
	bStepRequested = false;

	ResetSceneSelection();
	if (ViewportsPanel)
		ViewportsPanel->ClearGameViewportClient();
	DestroyWorldContext(PlayWorld);
	PlayWorld = nullptr;
	PIEViewIndex = InvalidViewIndex;
	PIEStartEditorViewIndex = InvalidViewIndex;
	DestroyPIEWindow();
	OnActiveWorldChanged();
}

bool UEditorEngine::CreatePIEWindow()
{
	constexpr int32 DefaultWidth = 1280;
	constexpr int32 DefaultHeight = 720;

	TUniquePtr<FWindow> NewWindow = MakeUnique<FWindow>();
	if (!NewWindow->Create(::GetModuleHandleW(nullptr), DefaultWidth, DefaultHeight, L"Hitori Engine - PIE"))
	{
		HTR_LOG(Error, "Failed to create PIE window. Falling back to the selected viewport.");
		return false;
	}
	// 닫기 버튼은 PIE만 끝내고, ImGui는 메인 창 입력만 받도록 훅을 끊는다.
	NewWindow->SetQuitOnClose(false);
	NewWindow->SetReceivesWndProcHook(false);

	PIEWindow = std::move(NewWindow);
	PIESwapchain = MakeUnique<FSwapchain>(GetEngineLoop().GetRenderDevice(), PIEWindow.get());
	PIETargetWidth = 0;
	PIETargetHeight = 0;
	PIEWindow->Show();
	return true;
}

void UEditorEngine::DestroyPIEWindow()
{
	if (!PIEWindow)
		return;

	PIETargetWidth = 0;
	PIETargetHeight = 0;
	bPIEWindowRendered = false;

	// Swapchain이 창 핸들을 출력 대상으로 쥐고 있으므로 창보다 먼저 놓는다.
	PIESwapchain.reset();
	PIEWindow->Destroy();
	PIEWindow.reset();

	// PIE 창이 포커스를 갖고 있었다면 에디터로 돌려준다.
	::SetForegroundWindow(MainWindow->GetHandle());
}

bool UEditorEngine::UpdatePIEWindowTargets()
{
	const uint32 Width = PIEWindow->GetWidth();
	const uint32 Height = PIEWindow->GetHeight();
	// 최소화 중에는 그릴 대상이 없다.
	if (Width == 0 || Height == 0 || ::IsIconic(PIEWindow->GetHandle()))
	{
		return false;
	}

	if (PIEWindow->CheckResized())
		PIESwapchain->Resize(Width, Height);

	if (Width != PIETargetWidth || Height != PIETargetHeight)
	{
		FWorldContext* Context = GetPIEWorldContext();
		if (!Context || !Context->GameViewport) return false;
		Context->GameViewport->Resize(Width, Height);
		if (!Context->GameViewport->GetViewTargets().IsValidFor(Width, Height))
		{
			return false;
		}

		PIETargetWidth = Width;
		PIETargetHeight = Height;
	}

	return true;
}

void UEditorEngine::CopyPIEWindowToBackbuffer()
{
	if (!PIESwapchain)
		return;

	FTexture2D* Backbuffer = PIESwapchain->GetBackbuffer();
	FWorldContext* Context = GetPIEWorldContext();
	FTexture2D* PIEColorTarget = Context && Context->GameViewport
		? Context->GameViewport->GetColorTarget() : nullptr;
	if (!Backbuffer || !PIEColorTarget ||
		Backbuffer->GetWidth() != PIETargetWidth ||
		Backbuffer->GetHeight() != PIETargetHeight)
		return;

	RenderCommand::GetContext()->CopyResource(Backbuffer->GetRawPtr(), PIEColorTarget->GetRawPtr());
	bPIEWindowRendered = true;
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection()
{
	Gizmo->SetTarget(nullptr, false);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot create a new scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::NewScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot open a scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::LoadScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveScene(EditorWorld);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveSceneAs(EditorWorld);
}
