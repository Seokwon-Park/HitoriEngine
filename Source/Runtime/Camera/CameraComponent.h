#pragma once

#include "UObject/Class.h"
#include "../Components/SceneComponent.h"
#include "../Math/EngineMath.h"
#include "../Math/Rotator.h"


class UCameraComponent : public USceneComponent
{
	DECLARE_CLASS(UCameraComponent, USceneComponent)

public:
	UCameraComponent() { PrimaryComponentTick.bCanEverTick = true; }   // 이동·회전 입력을 매 프레임 처리한다

	// 매 프레임 카메라 입력/이동 처리
	virtual void TickComponent(float DeltaTime) override;

	// Camera 행렬 계산
	FMatrix GetViewMatrix() const;
	FMatrix GetPerspectiveMatrix() const;
	FMatrix GetOrthogonalMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;

	// Picking
	FRay DeProjection(FVector2 MousePos, float ScreenW, float ScreenH);

	// Get & Set
	float GetFieldOfView() const { return FieldOfView; }
	void SetFieldOfView(float InFieldOfView) { FieldOfView = InFieldOfView; }

	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }

	float GetNearZ() const { return NearClipPlane; }
	void SetNearZ(float InNearZ) { NearClipPlane = InNearZ; }

	float GetFarZ() const { return FarClipPlane; }
	void SetFarZ(float InFarZ) { FarClipPlane = InFarZ; }

	bool GetIsOrthogonal() const { return bIsOrthogonal; }
	void SetIsOrthogonal(bool bInIsOrthographic) { bIsOrthogonal = bInIsOrthographic; }

	float GetOrthoWidth() const { return OrthoWidth; }
	void SetOrthoWidth(float InOrthoWidth) { OrthoWidth = InOrthoWidth; }

	// 마우스 조작 설정
	float GetMoveSpeed() const { return MoveSpeed; }
	void SetMoveSpeed(float InMoveSpeed) { MoveSpeed = InMoveSpeed; }

	float GetMouseSensitivity() const { return MouseSensitivity; }
	void SetMouseSensitivity(float InMouseSensitivity) { MouseSensitivity = InMouseSensitivity; }

	float GetWheelSpeed() const { return WheelSpeed; }
	void SetWheelSpeed(float InWheelSpeed) { WheelSpeed = InWheelSpeed; }
	// 외부 ViewportLayout가 View별 입력을 처리할 때 기본 카메라의 중복 입력 갱신을 끈다.
	void SetExternalInputManaged(bool bValue) { bExternalInputManaged = bValue; }
	bool IsExternalInputManaged() const { return bExternalInputManaged; }

	// 마우스 조작 업데이트
	void UpdateMovement(float DeltaTime); // 마우스 이동
	void UpdateRotation(float DeltaTime); // 마우스 우클릭 회전
	void UpdateZoom(float DeltaTime);     // 마우스 휠 줌

protected:
	float FieldOfView = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearClipPlane = 0.1f;
	float FarClipPlane = 10000.0f;

	bool bIsOrthogonal = false;
	float OrthoWidth = 16.0f;
	
	float MoveSpeed = 10.0f;
	float MouseSensitivity = 0.05f;
	float WheelSpeed = 0.1f;
	bool bExternalInputManaged = false;

};

