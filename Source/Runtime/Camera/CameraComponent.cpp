#include "EnginePCH.h"
#include "CameraComponent.h"
#include "Input/InputSystem.h"
#include "../Collision/Ray.h"

// 외부 입력 관리 시 기본 입력을 생략해 ViewportLayout와 카메라의 중복 갱신을 막는다.
void UCameraComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
	if (bExternalInputManaged)
		return;

    UpdateMovement(DeltaTime);
    UpdateRotation(DeltaTime);
    UpdateZoom(DeltaTime);
}

// 마우스 조작

// 키 입력을 카메라의 전방·우측·상방 이동으로 변환한다.
void UCameraComponent::UpdateMovement(float DeltaTime)
{
    if (FInputSystem::IsKeyDown(EKeyCode::W)) Transform.Location += Transform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::S)) Transform.Location -= Transform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::A)) Transform.Location -= Transform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::D)) Transform.Location += Transform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::Q)) Transform.Location -= Transform.GetUp() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::E)) Transform.Location += Transform.GetUp() * MoveSpeed * DeltaTime;
}

// 우클릭 중 마우스 변화량을 Euler Pitch·Yaw에 적용한다.
void UCameraComponent::UpdateRotation(float DeltaTime)
{
    if (!FInputSystem::IsMouseDown(EMouseButton::Right)) return;

    Transform.Rotation.Pitch += FInputSystem::GetMouseDeltaY() * MouseSensitivity;
    Transform.Rotation.Yaw += FInputSystem::GetMouseDeltaX() * MouseSensitivity;
}

// 휠 입력으로 원근 이동 또는 직교 폭을 조절한다.
void UCameraComponent::UpdateZoom(float DeltaTime)
{
    int32 WheelDelta = FInputSystem::GetWheelDelta();
    if (WheelDelta == 0) return;

    Transform.Location += Transform.GetForward() * WheelSpeed * WheelDelta * DeltaTime;
}


// 화면 좌표를 투영 모드에 맞는 월드 광선으로 역투영한다.
FRay UCameraComponent::DeProjection(FVector2 MousePos, float ScreenW, float ScreenH)
{
    // 마우스 화면 좌표를 NDC 좌표(-1 ~ 1)로 변환
    const float NDCX = 2.0f * MousePos.X / ScreenW - 1.0f;
    const float NDCY = 1.0f - 2.0f * MousePos.Y / ScreenH;


    // Projection / View 행렬의 역행렬
    const FMatrix InvProjection = GetProjectionMatrix().Inverse();
    const FMatrix InvView = GetViewMatrix().Inverse();


    // NDC 공간의 Near / Far 지점
    FVector4 NearPoint(NDCX, NDCY, 0.0f, 1.0f);
    FVector4 FarPoint(NDCX, NDCY, 1.0f, 1.0f);


    // Projection 역변환
    NearPoint = NearPoint * InvProjection;
    FarPoint = FarPoint * InvProjection;


    // Perspective Divide 복원
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;


    // View 역변환
    NearPoint = NearPoint * InvView;
    FarPoint = FarPoint * InvView;


    // Homogeneous 좌표 정리
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;

    FRay Ray;

    if (bIsOrthogonal)
    {
        Ray.Origin = FVector(NearPoint.X, NearPoint.Y, NearPoint.Z);
    }
    // 원근 투영은 모든 Ray가 카메라 위치에서 시작
    else
    {
        Ray.Origin = GetWorldLocation();
    }

    // 시작점에서 Far 지점을 향하는 방향을 Ray 방향으로 사용
    Ray.Direction = (FVector(FarPoint.X, FarPoint.Y, FarPoint.Z) - Ray.Origin).Normalized();

    return Ray;
}

// 카메라 Transform의 역변환으로 View 행렬을 만든다.
FMatrix UCameraComponent::GetViewMatrix() const
{
    //return GetWorldMatrix().Inverse();

    FVector WorldLocation = GetWorldLocation();
    FRotator WorldRotation = GetWorldRotation();

    FQuat WorldQuat = WorldRotation.Quaternion();
    FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    FMatrix InverseTranslation = FMatrix::MakeTranslation(WorldLocation * -1.0f);
    FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

// 시야각·종횡비·클리핑 거리로 원근 행렬을 만든다.
FMatrix UCameraComponent::GetPerspectiveMatrix() const
{
	const float HalfFOV = FMath::DegreesToRadians(FieldOfView) * 0.5f;

	const float YScale = 1.0f / tan(HalfFOV);
	const float XScale = YScale / AspectRatio;

    const float ZScale = FarClipPlane / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane * FarClipPlane /(FarClipPlane - NearClipPlane);

    //Forward가 X인 좌표
	return FMatrix(
		0.0f, 0.0f, ZScale, 1.0f,
		XScale, 0.0f, 0.0f, 0.0f,
		0.0f, YScale, 0.0f, 0.0f,
		0.0f, 0.0f, ZOffset, 0.0f
	);
}

// 직교 폭·종횡비·클리핑 거리로 직교 행렬을 만든다.
FMatrix UCameraComponent::GetOrthogonalMatrix() const
{
    const float Width = OrthoWidth;
    const float Height = Width / AspectRatio;

    const float XScale = 2.0f / Width;
    const float YScale = 2.0f / Height;

    const float ZScale = 1.0f / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane / (FarClipPlane - NearClipPlane);

    return FMatrix(
        0.0f, 0.0f, ZScale, 0.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 1.0f
    );
}

// 투영 모드에 대응하는 행렬을 선택한다.
FMatrix UCameraComponent::GetProjectionMatrix() const
{
    if (bIsOrthogonal)
        return GetOrthogonalMatrix();  // 직교 투영
     
    return GetPerspectiveMatrix();     // 원근 투영
}

// 행 벡터 규약으로 View와 Projection을 곱한다.
FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}
