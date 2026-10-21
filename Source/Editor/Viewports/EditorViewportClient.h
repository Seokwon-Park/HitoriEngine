#pragma once

#include "Engine/ViewportClient.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Render/SceneViewTargets.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"
#include "Render/Texture2D.h"

#include <functional>

class UWorld;
class FGizmo;
class AActor;


// 뷰포트 투영 타입
enum class ELevelViewportType
{
	Perspective,
	Top,
	Front,
	Right,
};

// 뷰포트 렌더 모드
enum class EViewportMode
{
	Solid,
	Wireframe,
	SceneDepth,
	WorldNormal,
	LightCulling,
};

// 에디터 뷰포트 클라이언트
class FEditorViewportClient : public FViewportClient {
public:
  FEditorViewportClient();
  virtual ~FEditorViewportClient() override = default;

  // 월드 조회 및 설정
  virtual UWorld *GetWorld() const override { return World; }
  void SetWorld(UWorld *InWorld) { if (World != InWorld) RenderData.ResetScene(); World = InWorld; }

  // 입력 이벤트
  virtual void MouseMove(FViewport *Viewport, int32 X, int32 Y) override;
  virtual bool InputKey(FViewport *Viewport, int32 Key, bool bDown) override;
  virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;

  // 뷰포트 상태
  int32 GetViewIndex() const { return ViewIndex; }
  void SetViewIndex(int32 InIndex) { ViewIndex = InIndex; }

  ELevelViewportType GetViewportType() const { return ViewportType; }
  void SetViewportType(ELevelViewportType InType);

  bool IsPerspective() const { return bIsPerspective; }
  void SetIsPerspective(bool bPerspective) { bIsPerspective = bPerspective; }

  bool IsRealtime() const { return bIsRealtime; }
  void SetRealtime(bool bRealtime) { bIsRealtime = bRealtime; }

  // 카메라 변환
  const FVector &GetViewLocation() const { return ViewLocation; }
  void SetViewLocation(const FVector &InLocation) { ViewLocation = InLocation; }

  const FRotator &GetViewRotation() const { return ViewRotation; }
  void SetViewRotation(const FRotator &InRotation) {
    ViewRotation = InRotation;
  }

  float GetViewFOV() const { return ViewFOV; }
  void SetViewFOV(float InFOV) { ViewFOV = InFOV; }

  float GetOrthoWidth() const { return OrthoWidth; }
  void SetOrthoWidth(float InWidth) { OrthoWidth = InWidth; }

  float GetAspectRatio() const { return AspectRatio; }
  void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }

  // 카메라 입력 처리
  void TickInput(float DeltaTime, bool bCaptured, bool bHovered,
                 int32 WheelDelta, float MoveSpeed, float MouseSensitivity);

  // 기즈모 조작과 피킹 처리
  void
  UpdateGizmoAndPicking(FGizmo *InGizmo, const FVector2 &LocalMousePosition,
                        const std::function<void(AActor *)> &OnSelectActor);

  // 행렬 계산
  FMatrix GetViewMatrix() const;
  FMatrix GetPerspectiveMatrix() const;
  FMatrix GetOrthogonalMatrix() const;
  FMatrix GetProjectionMatrix() const;
  FMatrix GetViewProjectionMatrix() const;

  // 카메라 전방 벡터
  FVector GetViewForward() const;

  // 씬 뷰 생성
  FSceneView CalcSceneView(const FRect &InViewRect) const;
  FSceneView CalcSceneView() const;

  // 화면 역투영 레이 생성
  FRay DeprojectScreenToWorld(const FVector2 &ScreenPos) const;

  // 뷰포트 영역 및 타깃
  const FRect &GetRect() const { return Rect; }
  void SetRect(const FRect &InRect) { Rect = InRect; }

  bool IsActive() const { return bActive; }
  void SetActive(const bool bInActive) { bActive = bInActive; }

  uint32 GetWidth() const { return Width; }
  uint32 GetHeight() const { return Height; }

  FTexture2D *GetColorTarget() const { return ViewTargets.SceneColor.get(); }
  FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output) { return ViewTargets.SwapSceneColorAndBind(Output); }
  FTexture2D *GetDepthTarget() const { return ViewTargets.Depth.get(); }
	FSceneRenderData& GetRenderData() { return RenderData; }
	FSceneViewTargets& GetViewTargets() { return ViewTargets; }
	const FSceneViewTargets &GetViewTargets() const { return ViewTargets; }

  bool IsWireframe() const { return ViewportMode == EViewportMode::Wireframe; }
  bool IsSceneDepth() const { return ViewportMode == EViewportMode::SceneDepth; }
  bool IsWorldNormal() const { return ViewportMode == EViewportMode::WorldNormal; }
  bool IsLightCulling() const { return ViewportMode == EViewportMode::LightCulling; }
  EViewportMode GetViewportMode() const { return ViewportMode; }
  void SetViewportMode(const EViewportMode InViewportMode) { ViewportMode = InViewportMode; }
  float GetMaxRange() const { return MaxRange; }
  void SetMaxRange(float InMaxRange) { MaxRange = InMaxRange; }

  // 타깃 크기 변경
  void Resize(uint32 InWidth, uint32 InHeight);

protected:
  UWorld *World = nullptr;
  int32 ViewIndex = 0;
  ELevelViewportType ViewportType = ELevelViewportType::Perspective;
  bool bIsPerspective = true;
  bool bIsRealtime = true;
  bool bActive = false;
  EViewportMode ViewportMode = EViewportMode::Solid;

  FRect Rect{};
  uint32 Width = 0;
  uint32 Height = 0;
	FSceneViewTargets ViewTargets;
	FSceneRenderData RenderData;

  FVector ViewLocation = FVector(0.0f, 0.0f, 0.0f);
  FRotator ViewRotation = FRotator(0.0f, 0.0f, 0.0f);
  float ViewFOV = 60.0f;
  float AspectRatio = 1.777778f;
  float NearClip = 0.1f;
  float FarClip = 10000.0f;
  float OrthoWidth = 10.0f;
  float MaxRange = 50.0f;
};
