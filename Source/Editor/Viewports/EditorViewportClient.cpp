#include "EnginePCH.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Components/PrimitiveComponent.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/InputSystem.h"
#include "Camera/ViewportCameraMovement.h"
#include "Math/EngineMath.h"
#include "Render/RenderCommand.h"

FEditorViewportClient::FEditorViewportClient() {}

void FEditorViewportClient::TickInput(float DeltaTime, bool bCaptured,
                                      bool bHovered, int32 WheelDelta,
                                      float MoveSpeed, float MouseSensitivity) {

  FViewportCameraInput Input;
  Input.bCaptured = bCaptured;
  Input.bAllowWheel = bCaptured || bHovered;
  Input.MouseX = static_cast<float>(FInputSystem::GetMouseDeltaX());
  Input.MouseY = static_cast<float>(FInputSystem::GetMouseDeltaY());
  Input.Wheel = static_cast<float>(WheelDelta);
  Input.bForward = FInputSystem::IsKeyDown(EKeyCode::W);
  Input.bBackward = FInputSystem::IsKeyDown(EKeyCode::S);
  Input.bRight = FInputSystem::IsKeyDown(EKeyCode::D);
  Input.bLeft = FInputSystem::IsKeyDown(EKeyCode::A);
  Input.bUp = FInputSystem::IsKeyDown(EKeyCode::E);
  Input.bDown = FInputSystem::IsKeyDown(EKeyCode::Q);
  FViewportCameraState Camera{ViewLocation, ViewRotation, OrthoWidth,
      Width > 0 ? static_cast<float>(Width) : 800.0f, bIsPerspective};
  ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);
  ViewLocation = Camera.Location;
  ViewRotation = Camera.Rotation;
  OrthoWidth = Camera.OrthoWidth;
}

// 기즈모 조작 및 액터 피킹 갱신
void FEditorViewportClient::UpdateGizmoAndPicking(
    FGizmo *InGizmo, const FVector2 &LocalMousePosition,
    const std::function<void(AActor *)> &OnSelectActor) {
  if (!bActive || !InGizmo) {
    return;
  }

  const FVector2 ViewLocalMouse(LocalMousePosition.X - Rect.X,
                                LocalMousePosition.Y - Rect.Y);
  FRay Ray = DeprojectScreenToWorld(ViewLocalMouse);
  const FMatrix ViewProjection = GetViewProjectionMatrix();
  const bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

  InGizmo->Update(Ray, ViewLocalMouse, ViewProjection,
                  static_cast<int>(Rect.Width), static_cast<int>(Rect.Height),
                  bMouseDown, GetViewLocation(), !IsPerspective());

  if (FInputSystem::IsMousePressed(EMouseButton::Left) && !InGizmo->IsUsing() &&
      InGizmo->GetHoveredAxis() < 0) {
    FHitResult Hit;
    if (World && World->LineTraceSingle(Ray, Hit)) {
      if (Hit.HitComponent && Hit.HitComponent->GetOwner()) {
        if (OnSelectActor) {
          OnSelectActor(Hit.HitComponent->GetOwner());
        }
      }
    } else {
      if (OnSelectActor) {
        OnSelectActor(nullptr);
      }
    }
  }
}

void FEditorViewportClient::MouseMove(FViewport *Viewport, int32 X, int32 Y) {}

bool FEditorViewportClient::InputKey(FViewport *Viewport, int32 Key,
                                     bool bDown) {
  return false;
}

bool FEditorViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) {
  return false;
}

// 뷰 행렬 계산
FMatrix FEditorViewportClient::GetViewMatrix() const {
  FQuat WorldQuat = ViewRotation.Quaternion();
  FMatrix RotationMatrix = WorldQuat.ToFMatrix();

  FMatrix InverseTranslation = FMatrix::MakeTranslation(ViewLocation * -1.0f);
  FMatrix InverseRotation = RotationMatrix.GetTransposed();

  return InverseTranslation * InverseRotation;
}

// 원근 투영 행렬
FMatrix FEditorViewportClient::GetPerspectiveMatrix() const {
  const float HalfFOV = FMath::DegreesToRadians(ViewFOV) * 0.5f;

  const float YScale = 1.0f / tan(HalfFOV);
  const float XScale = YScale / AspectRatio;

  const float ZScale = FarClip / (FarClip - NearClip);
  const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);

  return FMatrix(0.0f, 0.0f, ZScale, 1.0f, XScale, 0.0f, 0.0f, 0.0f, 0.0f,
                 YScale, 0.0f, 0.0f, 0.0f, 0.0f, ZOffset, 0.0f);
}

// 직교 투영 행렬
FMatrix FEditorViewportClient::GetOrthogonalMatrix() const {
  const float Width = OrthoWidth;
  const float Height = Width / AspectRatio;

  const float XScale = 2.0f / Width;
  const float YScale = 2.0f / Height;

  const float ZScale = 1.0f / (FarClip - NearClip);
  const float ZOffset = -NearClip / (FarClip - NearClip);

  return FMatrix(0.0f, 0.0f, ZScale, 0.0f, XScale, 0.0f, 0.0f, 0.0f, 0.0f,
                 YScale, 0.0f, 0.0f, 0.0f, 0.0f, ZOffset, 1.0f);
}

// 투영 행렬
FMatrix FEditorViewportClient::GetProjectionMatrix() const {
  if (bIsPerspective) {
    return GetPerspectiveMatrix();
  }
  return GetOrthogonalMatrix();
}

// 뷰 프로젝션 행렬
FMatrix FEditorViewportClient::GetViewProjectionMatrix() const {
  return GetViewMatrix() * GetProjectionMatrix();
}

// 전방 벡터 계산
FVector FEditorViewportClient::GetViewForward() const {
  FQuat WorldQuat = ViewRotation.Quaternion();
  return WorldQuat.GetForwardVector();
}

// 씬 뷰 정보 구성
FSceneView FEditorViewportClient::CalcSceneView(const FRect &InViewRect) const {
  FSceneView OutView;
  OutView.ViewIndex = ViewIndex;
  OutView.ViewLocation = ViewLocation;
  OutView.ViewRotation = ViewRotation;
  OutView.ViewForward = GetViewForward();
  OutView.ViewMatrix = GetViewMatrix();
  OutView.ProjectionMatrix = GetProjectionMatrix();
  OutView.ViewProjectionMatrix = GetViewProjectionMatrix();
  OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
  OutView.ViewRect = InViewRect;
  OutView.FieldOfView = ViewFOV;
  OutView.AspectRatio = AspectRatio;
  OutView.NearClip = NearClip;
  OutView.FarClip = FarClip;
  OutView.OrthoWidth = OrthoWidth;
  OutView.bIsPerspective = bIsPerspective;
  OutView.bIsWireframe = ViewportMode == EViewportMode::Wireframe;
  return OutView;
}

// 씬 뷰 생성
FSceneView FEditorViewportClient::CalcSceneView() const {
  return CalcSceneView(
      FRect(0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height)));
}

// 뷰포트 크기 변경 및 타깃 생성
void FEditorViewportClient::Resize(const uint32 InWidth,
                                   const uint32 InHeight) {
  if (Width == InWidth && Height == InHeight &&
      ViewTargets.IsValidFor(InWidth, InHeight)) {
    return;
  }

  const uint32 OldWidth = Width;
  const uint32 OldHeight = Height;

  Width = InWidth;
  Height = InHeight;

  ViewTargets.Resize(Width, Height);

  if (Height > 0) {
    SetAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
  }

  if (!bIsPerspective && OldWidth > 1 && Width > 1) {
    // 화면 크기 변경 시 직교 배율 유지
    OrthoWidth *= static_cast<float>(Width) / static_cast<float>(OldWidth);
  }
}

// 뷰포트 시점 설정
void FEditorViewportClient::SetViewportType(const ELevelViewportType InType) {
  ViewportType = InType;
  switch (InType) {
  case ELevelViewportType::Perspective:
    bIsPerspective = true;
    ViewLocation = FVector(-5.0f, -10.0f, 10.0f);
    ViewRotation = FRotator(30.0f, 45.0f, 0.0f);
    break;
  case ELevelViewportType::Top:
    bIsPerspective = false;
    ViewLocation = FVector(0.0f, 0.0f, 20.0f);
    ViewRotation = FRotator(90.0f, 0.0f, 0.0f);
    break;
  case ELevelViewportType::Front:
    bIsPerspective = false;
    ViewLocation = FVector(20.0f, 0.0f, 0.0f);
    ViewRotation = FRotator(0.0f, 180.0f, 0.0f);
    break;
  case ELevelViewportType::Right:
    bIsPerspective = false;
    ViewLocation = FVector(0.0f, 20.0f, 0.0f);
    ViewRotation = FRotator(0.0f, -90.0f, 0.0f);
    break;
  }
}

// 화면 좌표 역투영
FRay FEditorViewportClient::DeprojectScreenToWorld(
    const FVector2 &ScreenPos) const {
  if (Width == 0 || Height == 0) {
    return FRay();
  }
  const float ScreenWidth = static_cast<float>(Width);
  const float ScreenHeight = static_cast<float>(Height);
  const float NdcX = (ScreenPos.X / ScreenWidth) * 2.0f - 1.0f;
  const float NdcY = 1.0f - (ScreenPos.Y / ScreenHeight) * 2.0f;
  const float Aspect =
      AspectRatio > 0.0f ? AspectRatio : (ScreenWidth / ScreenHeight);
  const FQuat Rotation = ViewRotation.Quaternion();
  const FVector Forward = Rotation.GetForwardVector();
  const FVector Right = Rotation.GetRightVector();
  const FVector Up = Rotation.GetUpVector();

  if (bIsPerspective) {
    const float Tangent = tanf(FMath::DegreesToRadians(ViewFOV) * 0.5f);
    const FVector Direction =
        (Forward + Right * (NdcX * Tangent * Aspect) + Up * (NdcY * Tangent))
            .Normalized();
    return FRay(ViewLocation, Direction);
  } else {
    const float OrthoHeight = OrthoWidth / Aspect;
    const FVector Origin = ViewLocation + Forward * NearClip +
                           Right * (NdcX * OrthoWidth * 0.5f) +
                           Up * (NdcY * OrthoHeight * 0.5f);
    return FRay(Origin, Forward.Normalized());
  }
}
