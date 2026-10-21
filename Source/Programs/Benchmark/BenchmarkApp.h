//#pragma once
//
//#include "Engine/GameEngine.h"
//#include "Engine/World.h"
//
//#include "Editor/EditorUI/EditorUI.h"
//#include "Editor/Gizmo/Gizmo.h"
//#include "Editor/Gizmo/GizmoRenderer.h"
//#include "Editor/Rendering/GridRenderer.h"
//#include "Editor/Rendering/Outline.h"
//#include "Editor/Rendering/OutLineRenderer.h"
//#include "Editor/Settings/SettingsPanel.h"
//#include "Render/Renderer.h"
//#include "Render/LineBatcher.h"
//
//#include "Editor/EditorUI/ImGuiRenderer.h"
//class FOutlinerPanel;
//class FDetailsPanel;
//class FEditorControlsPanel;
//class FOutputLogPanel;
//class UPrimitiveComponent;
//
//class UBenchmarkEngine : public UGameEngine
//{
//	DECLARE_CLASS(UBenchmarkEngine, UGameEngine)
//
//public:
//	FEngineConfig GetConfig() const override;
//
//	bool Init() override;
//	void Tick(float DeltaTime) override;
//	void PreExit() override;
//
//private:
//	// ImGui로 프로파일링 오버레이를 그린다.
//	void DrawProfileOverlay();
//
//	// 에디터 패널·기즈모·아웃라인·그리드를 단일 View 기준으로 준비한다.
//	void InitEditorTools();
//	// 마우스 Ray로 Gizmo를 갱신하고, 축을 잡지 않은 클릭은 피킹으로 처리한다.
//	void UpdateGizmoAndPicking();
//	// Outliner·Gizmo·Outline·Details의 선택 대상을 한 번에 맞춘다.
//	void SelectScene(USceneComponent* Root);
//
//	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
//
//	// 패널 소유권은 EditorUI에 있고 여기서는 raw pointer만 보관한다.
//	TUniquePtr<FEditorUI> EditorUI;
//	FOutlinerPanel* OutlinerPanel = nullptr;
//	FDetailsPanel* DetailsPanel = nullptr;
//	FEditorControlsPanel* EditorControlsPanel = nullptr;
//	FOutputLogPanel* OutputLogPanel = nullptr;
//
//	TUniquePtr<FGizmo> Gizmo;
//	TUniquePtr<FGizmoRenderer> GizmoRenderer;
//	TUniquePtr<FOutline> Outline;
//	TUniquePtr<FOutlineRenderer> OutlineRenderer;
//	TUniquePtr<FGridRenderer> GridRenderer;
//	// 선택된 물체 하나의 바운딩 박스만 그린다. BuildVertexBuffer(전체 순회)는 쓰지 않는다.
//	TUniquePtr<FLineBatcher> LineBatcher;
//
//	// 선택된 물체의 UUID·이름을 오버레이에 표시하고 바운딩 박스를 그린다.
//	UPrimitiveComponent* GetSelectedPrimitive() const;
//	void DrawSelectionBounds(const FMatrix& ViewProjection);
//
//	// Benchmark는 SettingsPanel을 두지 않으므로 Grid 옵션만 기본값으로 보관한다.
//	FEditorSettings GridSettings;
//
//	// 지역 변수면 매 프레임 수 MB를 새로 할당·해제해 Gather 시간이 튄다.
//	FRenderQueue RenderQueue;
//
//	// 오버레이 버튼으로 요청하면 다음 프레임 한 번만 오클루전 가시 비율을 측정한다.
//	bool bMeasureOcclusionRequested = false;
//	FOcclusionMeasureResult LastOcclusionMeasure;
//
//	UWorld* World;
//};
