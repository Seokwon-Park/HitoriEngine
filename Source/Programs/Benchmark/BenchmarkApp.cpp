#include "EnginePCH.h"
#include "BenchmarkApp.h"

#if defined(BENCHMARK)

#include "Asset/AssetManager.h"
#include "Core/Window.h"
#include "Core/EngineLog.h"
#include "Core/SplashScreen.h"
#include "Core/Stats/LightweightStats.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"

#include "GameFramework/Actor/StaticMeshActor.h"

#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Serialization/JsonArchive.h"

#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/HitoriEd/EditorFileUtils.h"

#include "Input/InputSystem.h"
#include "Collision/HitResult.h"
#include "Core/Stats/EditorStats.h"

namespace
{
	// 숫자 셀 오른쪽 정렬
	void DrawTimeCell(double Milliseconds);
}

DECLARE_CYCLE_STAT("ImGui Render", STAT_ImGuiRender);
DECLARE_CYCLE_STAT("Gather (Total)", STAT_GatherTotal);
DECLARE_CYCLE_STAT("Render Opaque (Total)", STAT_RenderOpaqueTotal);

FEngineConfig UBenchmarkEngine::GetConfig() const
{
	FEngineConfig Desc = Super::GetConfig();
	Desc.Title = L"Benchmarker";
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

bool UBenchmarkEngine::Init()
{
	if (!Super::Init())
		return false;

	World = UWorld::CreateWorld(EWorldType::Game, true);

	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(GetEngineLoop().GetMainWindow()->GetHandle(), GetEngineLoop().GetRenderDevice()->GetDevice(), GetEngineLoop().GetRenderDevice()->GetContext()))
	{
		return false;
	}

	// 이 사과 OBJ들은 Z-up으로 만들어져 있어, 임포터의 OBJ 표준(Y-up) -> 엔진(Z-up) 변환을 거치면 꼭지가 -X로 눕는다.
	// Y축 기준 회전으로 -X -> +Z가 되게 세운다 (LOD·피킹 BVH가 새 방향으로 만들어지도록 LOD 생성 전에 한다).
	const auto StandUp = [](const FVector& V) { return FVector(V.Z, V.Y, -V.X); };
	for (const char* ApplePath : { "Assets/Data/apple_mid.obj", "Assets/Data/bitten_apple_mid.obj" })
	{
		if (UStaticMesh* Apple = UAssetManager::LoadObjStaticMesh(ApplePath))
			UAssetManager::ReorientStaticMesh(*Apple, StandUp);
	}

	FLODGenerateRequest Request;
	// 화면 절반 높이 대비 반지름 비율. 2560x1600 기준 반지름 약 96px·32px 아래에서 LOD2·LOD3으로 전환한다.
	// 가까이 가면 사과 대부분이 LOD1(약 2.4k 삼각형)로 잡혀 GPU가 삼각형 처리에 묶이던 것을 줄인다.
	Request.ScreenThresholds = { 0.4f, 0.12f, 0.04f };
	Request.bSaveToAsset = false; // 현재 저장 경로가 미구현

	auto GenerateFor = [&](UStaticMesh* Asset)
		{
			const FLODGenerateResult Report = UAssetManager::Get().GenerateStaticMeshLODs(*Asset, Request);
			const FString Diagnostics = std::format(
				"[Benchmark] {}: LOD {}. target={}/{}/{}/{}, actual={}/{}/{}/{}, volume={:.3f}/{:.3f}/{:.3f}/{:.3f}, feature={}, topology={}, flips={}, error={}, reason={}\n",
				Asset->GetPath(), Report.bSuccess ? "OK" : "FAILED",
				Report.TargetTriangles[0], Report.TargetTriangles[1],
				Report.TargetTriangles[2], Report.TargetTriangles[3],
				Report.ActualTriangles[0], Report.ActualTriangles[1],
				Report.ActualTriangles[2], Report.ActualTriangles[3],
				Report.VolumeRatio[0], Report.VolumeRatio[1],
				Report.VolumeRatio[2], Report.VolumeRatio[3],
				Report.RejectedFeatureEdges, Report.RejectedTopology,
				Report.RejectedFlips, Report.RejectedError, Report.FailureReason);
			OutputDebugStringA(Diagnostics.c_str());

			if (!Report.bSuccess)
			{
				HTR_LOG(Warning, "LOD generation failed for {}: {}", Asset->GetPath(), Report.FailureReason);
				return;
			}

			HTR_LOG(Info, "{} LOD triangles: {} / {} / {} / {}",
				Asset->GetPath(),
				Report.ActualTriangles[0],
				Report.ActualTriangles[1],
				Report.ActualTriangles[2],
				Report.ActualTriangles[3]);
		};

	// Assets 아래 OBJ 메시는 시작할 때 전부 로드되므로, 모두 LOD를 만들어 두면 어떤 씬을 열어도 LOD가 적용된다.
	UAssetManager::ForEachObjStaticMesh([&](const FString& Key, UStaticMesh& Asset)
		{
			if (Asset.GetLODCount() == 1)
			{
				FSplashScreen::SetText("Generating LODs: " + Key);
				GenerateFor(&Asset);
			}
		});

	FSplashScreen::SetText(L"Loading Scene...");
	FSplashScreen::SetProgress(0.9f);

	// 실행 인자로 씬 경로를 받으면 그 씬을, 없으면 기본 씬을 연다. 예: Benchmark.exe Scenes/Bocchi.scene
	FString ScenePath = "Scenes/Default.scene";
	if (__argc > 1 && __argv[1] && __argv[1][0] != '\0')
		ScenePath = __argv[1];
	// 카메라 위치·회전·FOV·클립은 씬 파일의 PerspectiveCamera에서 읽는다.
	if (!FJsonArchive::LoadWorld(World, ScenePath))
		HTR_LOG(Warning, "Failed to load scene: {}", ScenePath);

	InitEditorTools();

	return true;
}

// 에디터 패널·기즈모·아웃라인·그리드를 단일 View 기준으로 준비한다.
void UBenchmarkEngine::InitEditorTools()
{
	FRenderer* Renderer = GetEngineLoop().GetRenderer();

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, World);

	Gizmo = MakeUnique<FGizmo>();
	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Outline = MakeUnique<FOutline>();
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init(true, true);

	// File 메뉴로 씬을 바꾼다. 기존 액터가 지워지므로 선택(기즈모·아웃라인·디테일)을 먼저 비운다.
	// 아웃라이너 선택을 비우면 콜백으로 기즈모·아웃라인·디테일 선택도 함께 비워진다.
	EditorUI->SetOpenSceneCallback([this]()
		{
			OutlinerPanel->SelectActor(nullptr);
			DetailsPanel->SelectComponent(nullptr);
			FEditorFileUtils::LoadScene(World);
		});
	EditorUI->SetNewSceneCallback([this]()
		{
			OutlinerPanel->SelectActor(nullptr);
			DetailsPanel->SelectComponent(nullptr);
			FEditorFileUtils::NewScene(World);
		});

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);

	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	DetailsPanel->SetWorld(World);

	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	EditorControlsPanel->SetWorld(World);
	EditorControlsPanel->SetGizmo(Gizmo.get());

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(World);
	OutlinerPanel->SetSelectionCallback(
		[this](USceneComponent* Scene) { SelectScene(Scene); });

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			// 선택된 물체를 지우면 선택 표시(UUID·박스)가 해제된 포인터를 읽지 않도록 먼저 비운다.
			if (UPrimitiveComponent* Selected = GetSelectedPrimitive(); Selected && Selected->GetOwner() == Actor)
				SelectScene(nullptr);
			World->DestroyActor(Actor);
		});

	HTR_LOG(Info, "Benchmark editor tools ready.");
}

// Outliner·Gizmo·Outline·Details의 선택 대상을 한 번에 맞춘다.
void UBenchmarkEngine::SelectScene(USceneComponent* Root)
{
	Gizmo->SetTarget(Root);

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Root);
	if (!Primitive && Root && Root->GetOwner())
	{
		for (UActorComponent* C : Root->GetOwner()->GetComponents())
			if ((Primitive = Cast<UPrimitiveComponent>(C))) break;
	}
	Outline->SetTarget(Primitive);
	DetailsPanel->SetTarget(Root ? Root->GetOwner() : nullptr);
}

UPrimitiveComponent* UBenchmarkEngine::GetSelectedPrimitive() const
{
	return Gizmo ? Cast<UPrimitiveComponent>(Gizmo->GetTarget()) : nullptr;
}

// 선택된 물체 하나의 월드 AABB만 그린다. 컬링·피킹에 실제로 쓰는 프록시 바운드와 같다.
void UBenchmarkEngine::DrawSelectionBounds(const FMatrix& ViewProjection)
{
	UPrimitiveComponent* Selected = GetSelectedPrimitive();
	if (!Selected || !LineBatcher)
		return;

	FBox Bounds;
	if (const FPrimitiveSceneProxy* Proxy = Selected->GetSceneProxy())
	{
		const FAABB& WorldBounds = Proxy->GetBounds();
		Bounds = FBox{ WorldBounds.Center - WorldBounds.Extent, WorldBounds.Center + WorldBounds.Extent };
	}
	else
	{
		Bounds = Selected->CalcBounds();
	}

	LineBatcher->BeginFrame();
	LineBatcher->AddBox(Bounds, FVector4(1.0f, 1.0f, 0.0f, 1.0f));
	LineBatcher->OnRender(ViewProjection);

	// 라인 파이프라인이 바꾼 상태를 장면 기준으로 되돌린다.
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// 마우스 Ray로 Gizmo를 갱신하고, 축을 잡지 않은 클릭은 피킹으로 처리한다.
void UBenchmarkEngine::UpdateGizmoAndPicking()
{
	// ImGui 패널 위에서의 클릭은 씬 조작으로 넘기지 않는다.
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();

	const FVector2 MousePosition(
		static_cast<float>(FInputSystem::GetMouseX()),
		static_cast<float>(FInputSystem::GetMouseY()));

	const FRay Ray = Camera->DeProjection(
		MousePosition, static_cast<float>(Width), static_cast<float>(Height));

	Gizmo->Update(
		Ray,
		MousePosition,
		Camera->GetViewProjectionMatrix(),
		static_cast<int>(Width),
		static_cast<int>(Height),
		FInputSystem::IsMouseDown(EMouseButton::Left),
		Camera->GetWorldLocation(),
		Camera->GetIsOrthogonal());

	// 기즈모 축을 집고 있는 중이면 피킹으로 선택을 바꾸지 않는다.
	if (FInputSystem::IsMousePressed(EMouseButton::Left) &&
		!Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0)
	{
		FHitResult Hit;
		SelectScene(World->LineTraceSingle(Ray, Hit) ? Hit.HitComponent : nullptr);
	}
}

void UBenchmarkEngine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();
	Camera->SetAspectRatio(static_cast<float>(Width) / Height);

	// 기즈모 조작 결과가 같은 프레임의 UpdateAllTransforms에 반영되도록 World Tick보다 앞에 둔다.
	UpdateGizmoAndPicking();

	World->Tick(DeltaTime);

	EditorControlsPanel->DeltaTime = DeltaTime;
	EditorUI->Tick(DeltaTime);

	const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
	const FMatrix Projection = Camera->GetProjectionMatrix();
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ViewProjection);


	const float ScaleX = Projection.M[1][0];
	const float ScaleY = Projection.M[2][1];

	// LOD에 카메라 정보 저장
	FLODViewContext LODView{ Width, Height };
	LODView.ViewProjection = ViewProjection;
	LODView.CameraPosition = Camera->GetWorldLocation();
	LODView.CameraForward = Camera->GetWorldRotation()
		.Quaternion()
		.RotateVector(FVector(1.0f, 0.0f, 0.0f))
		.Normalized();
	LODView.ProjectionScaleSquared =
		std::max(ScaleX * ScaleX, ScaleY * ScaleY);
	LODView.NearZ = Camera->GetNearZ();
	LODView.bOrthographic = Camera->GetIsOrthogonal();
	LODView.Prepare();

	// 멤버 큐를 재사용한다. Renderer와 swap으로 버퍼를 주고받으므로 두 버퍼 모두 용량이 유지된다.
	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	{
		// 프러스텀 컬링 + GPU 오클루전 + Gather 전체
		SCOPE_CYCLE_COUNTER(STAT_GatherTotal);
		RenderQueue.Reset();
		World->GatherRenderPackets(RenderQueue, &LODView, &Frustum, Renderer);
	}

	GetEngineLoop().BeginBackbufferPass();

	const FViewportSettings Viewport{ 0, 0, Width, Height, 0.0f, 1.0f };
	const FVector CameraLocation = Camera->GetWorldLocation();
	const FVector CameraForward = Camera->GetTransform().GetForward();

	// 반투명이 Grid 위에 합성되도록 불투명 -> Grid -> 반투명 순서로 그린다.
	{
		SCOPE_CYCLE_COUNTER(STAT_RenderOpaqueTotal);
		Renderer->RenderQueueSorting(RenderQueue, ViewProjection, CameraLocation);
		Renderer->RenderOpaque(ViewProjection);
	}

	// Grid가 깊이를 쓰기 전, 불투명만 그려진 깊이 버퍼로 측정한다.
	if (bMeasureOcclusionRequested)
	{
		bMeasureOcclusionRequested = false;
		LastOcclusionMeasure = Renderer->MeasureOpaqueOcclusion(ViewProjection);
	}

	GridRenderer->OnRenderBatchGrid(
		ViewProjection,
		CameraLocation,
		CameraForward,
		EGridPlane::XY,
		static_cast<float>(GridSettings.GridSpacing),
		true,
		Viewport);

	{
		// Grid 파이프라인이 바꾼 상태를 장면 기준으로 되돌린다.
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Renderer->RenderTranslucent(ViewProjection);

		DrawSelectionBounds(ViewProjection);

		if (Outline->GetTarget())
			OutlineRenderer->OnRender(*Outline, ViewProjection, Viewport);

		if (Gizmo->GetTarget())
		{
			// 에디터와 같이 깊이를 비워 기즈모가 사과에 가려지지 않고 항상 위에 보이게 한다.
			// 아웃라인(스텐실)은 이미 그렸고 이후 장면 패스가 없으므로 지워도 된다.
			RenderCommand::ClearDepthStencil(GetEngineLoop().GetDepthBuffer());
			GizmoRenderer->OnRender(*Gizmo, ViewProjection, CameraLocation, Camera->GetIsOrthogonal());
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGuiRender);
		ImGuiRenderer->Begin();
		EditorUI->OnRender();
		DrawProfileOverlay();
		ImGuiRenderer->End();
	}

	GetEngineLoop().EndBackbufferPass();
	FStatRegistry::EndFrame();
}

void UBenchmarkEngine::PreExit()
{
	Super::PreExit();

	if (ImGuiRenderer)
	{
		ImGuiRenderer->Shutdown();
		ImGuiRenderer.reset();
	}
}

void UBenchmarkEngine::DrawProfileOverlay()
{
	const FFrameStats& Stats = GetEngineLoop().GetFrameStats();

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->WorkPos, ImGuiCond_Always);
	if (ImGui::Begin("Profile", nullptr, Flags))
	{
		ImGui::Text("Resolution: %u x %u", GetEngineLoop().GetViewportWidth(), GetEngineLoop().GetViewportHeight());
		ImGui::Text("FPS: %.1f (%.2f ms)", Stats.AverageFPS, Stats.AverageFrameMs);
		ImGui::Text("Frame Time: %.2f ms", Stats.AverageFrameMs);

		const FRenderStats& RS = World->GetRenderStats();
		ImGui::Text("Primitives: %u / %u visible", RS.VisiblePrimitives, RS.TotalPrimitives);
		ImGui::Text("Draw Calls: %u", RS.DrawCalls);
		ImGui::Text("Triangles: %.2f M", RS.Triangles / 1'000'000.0);

		if (ImGui::BeginTable("LODStats", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("LOD");
			ImGui::TableSetupColumn("Objects");
			ImGui::TableSetupColumn("Triangles");
			ImGui::TableSetupColumn("Tri %");
			ImGui::TableHeadersRow();
			for (int i = 0; i < 4; ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("LOD%d", i);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", RS.LODCounts[i]);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f M", RS.LODTriangles[i] / 1'000'000.0);
				ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f %%", RS.Triangles ? 100.0 * RS.LODTriangles[i] / RS.Triangles : 0.0);
			}
			ImGui::EndTable();
		}

		// 끄면 스코프 타이머가 사이클을 읽지 않고 표도 그리지 않는다. 피킹 시간은 계속 잰다.
		bool bProfileEnabled = FStatRegistry::IsEnabled();
		if (ImGui::Checkbox("CPU Profile", &bProfileEnabled))
		{
			FStatRegistry::SetEnabled(bProfileEnabled);
			FStatRegistry::Reset();   // 꺼져 있던 동안의 빈 구간이 평균에 섞이지 않도록
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
		{
			FStatRegistry::Reset();
		}

		constexpr ImGuiTableFlags TableFlags =
			ImGuiTableFlags_BordersInnerH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingFixedFit;

		if (bProfileEnabled && ImGui::BeginTable("CPUStats", 4, TableFlags))
		{
			ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 190.0f);
			ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Avg (120f)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableHeadersRow();

			for (const auto& [Name, Data] : FStatRegistry::GetAll())
			{
				if (TStatId{ Name } == EditorStats::STAT_PickingTime)
					continue;

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Name);

				ImGui::TableSetColumnIndex(1);
				DrawTimeCell(Data.GetLastMs());

				ImGui::TableSetColumnIndex(2);
				DrawTimeCell(Data.GetRecentAverageMs());

				ImGui::TableSetColumnIndex(3);
				DrawTimeCell(Data.GetMaxMs());
			}

			ImGui::EndTable();
		}

		if (const FCycleStatData* PickingData = FStatRegistry::Find(EditorStats::STAT_PickingTime))
		{
			ImGui::Text("Picking Time - Last: %.4f ms, Attempts: %d, Acc.: %.4f ms", PickingData->GetLastMs(), PickingData->CallCount, PickingData->GetTotalMs());
		}

		if (UPrimitiveComponent* Selected = GetSelectedPrimitive())
		{
			if (AActor* Owner = Selected->GetOwner())
				ImGui::Text("Selected: %s (UUID %u)", Owner->GetName().c_str(), Owner->GetUUID());
			else
				ImGui::Text("Selected: %s (UUID %u)", Selected->GetName().c_str(), Selected->GetUUID());
		}
		else
		{
			ImGui::TextUnformatted("Selected: None");
		}

		// 측정 전용: 불투명 물체 중 최종 화면에 픽셀을 남긴 비율 = 오클루전 컬링으로 얻을 수 있는 상한
		if (ImGui::SmallButton("Measure Occlusion"))
		{
			bMeasureOcclusionRequested = true;
		}
		if (const FOcclusionMeasureResult& M = LastOcclusionMeasure; M.bValid)
		{
			const auto Percent = [](uint64 Part, uint64 Whole) { return Whole ? 100.0 * Part / Whole : 0.0; };
			ImGui::Text("Visible Objects: %u / %u (%.1f %%) -> occluded %.1f %%",
				M.VisibleObjects, M.TotalObjects, Percent(M.VisibleObjects, M.TotalObjects),
				100.0 - Percent(M.VisibleObjects, M.TotalObjects));
			ImGui::Text("Visible Draws: %u / %u", M.VisibleDraws, M.TotalDraws);
			ImGui::Text("Visible Triangles: %.2f M / %.2f M (%.1f %%)",
				M.VisibleTriangles / 1'000'000.0, M.TotalTriangles / 1'000'000.0,
				Percent(M.VisibleTriangles, M.TotalTriangles));
			ImGui::Text("Measure cost: %.1f ms", M.ElapsedMs);
			if (M.GPUOccludedDraws > 0)
				ImGui::Text("GPU occluded draws: %u, False culls: %u (must be 0)", M.GPUOccludedDraws, M.FalseCulls);
		}

		// GPU 오클루전 컬링: 가림막 깊이 -> Hi-Z -> 물체별 판정 -> CPU로 읽어 Gather 전에 거름
		FGPUOcclusion& GPUOcclusion = GetEngineLoop().GetRenderer()->GetGPUOcclusion();
		FGPUOcclusionSettings& Occlusion = GPUOcclusion.GetSettings();
		ImGui::Checkbox("GPU Occlusion Culling", &Occlusion.bEnabled);
		if (Occlusion.bEnabled)
		{
			ImGui::SameLine();
			// 끄면 판정만 하고 그대로 그린다. 이 상태에서 Measure Occlusion을 누르면 잘못 가린 수를 검증한다.
			ImGui::Checkbox("Cull", &Occlusion.bCull);
			const FGPUOcclusionStats& OS = GPUOcclusion.GetStats();
			ImGui::Text("Occluders: %u (%.2f M tris, coverage %.1f screens)", OS.Occluders, OS.OccluderTriangles / 1'000'000.0, OS.Coverage);
			ImGui::Text("Occluded: %u / %u (%.1f %%), GPU wait: %.3f ms", OS.Occluded, OS.Tested,
				OS.Tested ? 100.0 * OS.Occluded / OS.Tested : 0.0, OS.WaitMs);
			ImGui::SetNextItemWidth(200.0f);
			ImGui::SliderFloat("Occluder Coverage", &Occlusion.OccluderCoverage, 0.5f, 64.0f, "%.1f screens", ImGuiSliderFlags_Logarithmic);
			int MaxOccluders = static_cast<int>(Occlusion.MaxOccluders);
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::SliderInt("Max Occluders", &MaxOccluders, 0, 16384))
				Occlusion.MaxOccluders = static_cast<uint32>(MaxOccluders);
		}
	}
	ImGui::End();
	ImGui::PopStyleColor(1);
}

namespace
{
	void DrawTimeCell(double Milliseconds)
	{
		char Text[32];
		snprintf(Text, sizeof(Text), "%.2f ms", Milliseconds);

		const float TextWidth = ImGui::CalcTextSize(Text).x;
		const float AvailableWidth = ImGui::GetContentRegionAvail().x;

		if (TextWidth < AvailableWidth)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - TextWidth));
		}

		ImGui::TextUnformatted(Text);
	}
}

#endif
