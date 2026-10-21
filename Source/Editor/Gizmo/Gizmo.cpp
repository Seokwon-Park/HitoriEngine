#include "EnginePCH.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Actor/LightActorBase.h"

static const FVector AxisDirs[3] = {
	FVector(1, 0, 0),
	FVector(0, 1, 0),
	FVector(0, 0, 1),
};

static const int AXIS_PLANE_YZ = 3;
static const int AXIS_PLANE_ZX = 4;
static const int AXIS_PLANE_XY = 5;
static const int AXIS_SCREEN = 6;   // 3축 자유 이동

static const float AxisLength = 1.5f;
static const float HitPixels = 12.0f;

// 입력 View의 카메라 조건으로 축 선택과 드래그를 갱신한다.
void FGizmo::Update(const FRay& MouseRay, const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH, bool bMouseDown, const FVector& CameraLocation, const bool bCameraOrthographic)
{
	ViewCameraLocation = CameraLocation;
	bViewCameraOrthographic = bCameraOrthographic;

	if (!Target) // 만약 현재 타겟이 없다면 
	{
		HoveredAxis = -1;
		DraggingAxis = -1;
		return;
	}

	if (DraggingAxis >= 0) // 만약 드래그 중인 축이 있다면
	{
		if (bMouseDown)
			UpdateDrag(MouseRay, MousePos);
		else
			EndDrag();
		return;
	}

	HoveredAxis = PickAxis(MousePos, ViewProj, ScreenW, ScreenH); // 현재 마우스가 올라간 기즈모 축이 있으면

	if (bMouseDown && HoveredAxis >= 0) // 마우스가 올라간 축이 있고 마우스가 눌렸으면
		BeginDrag(HoveredAxis, MouseRay, MousePos);


}

// 모드에 따라 선형 축 또는 회전 링 피킹을 선택한다.
int FGizmo::PickAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	if (Mode == EGizmoMode::Rotation)
		return PickRotationAxis(MousePos, ViewProj, ScreenW, ScreenH);

	return PickLinearAxis(MousePos, ViewProj, ScreenW, ScreenH);
}

// 투영한 축 선분과 마우스의 화면 거리로 조작 축을 고른다.
int FGizmo::PickLinearAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	FVector Origin = GetRenderLocation();

	FVector2 Center = WorldToScreen(Origin, ViewProj, ScreenW, ScreenH);
	FVector2 d = MousePos - Center;
	if (sqrtf(d.X * d.X + d.Y * d.Y) < 15.0f)
		return AXIS_SCREEN;

	int Best = -1;
	float BestDist = HitPixels;

	for (int i = 0; i < 3; ++i)
	{
		FVector2 Start = WorldToScreen(Origin, ViewProj, ScreenW, ScreenH);
		FVector2 End = WorldToScreen(Origin + GetAxisDirection(i) * AxisLength, ViewProj, ScreenW, ScreenH);

		float Dist = DistanceToSegment(MousePos, Start, End);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

// 회전 링을 선분으로 나누어 화면 거리로 회전 축을 고른다.
int FGizmo::PickRotationAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	const FVector Origin = GetRenderLocation();
	const int Segments = 32;

	int Best = -1;
	float BestDist = HitPixels;

	for (int Axis = 0; Axis < 3; ++Axis)
	{
		FVector u = GetAxisDirection((Axis + 1) % 3);
		FVector v = GetAxisDirection((Axis + 2) % 3);

		FVector2 prev;
		bool bHasPrev = false;

		for (int s = 0; s <= Segments; ++s)
		{
			float theta = (float)s / Segments * 2.0f * PI;
			FVector worldPos = Origin
				+ u * (RingRadius * cosf(theta))
				+ v * (RingRadius * sinf(theta));

			FVector2 screenPos = WorldToScreen(worldPos, ViewProj, ScreenW, ScreenH);

			if (bHasPrev)
			{
				float Dist = DistanceToSegment(MousePos, prev, screenPos);
				if (Dist < BestDist)
				{
					BestDist = Dist;
					Best = Axis;
				}
			}

			prev = screenPos;
			bHasPrev = true;
		}
	}

	FVector CamPos = GetCameraLocation();
	FVector Forward = (Origin - CamPos).Normalized();
	FVector u = FVector(0, 0, 1).Cross(Forward).Normalized();
	FVector v = Forward.Cross(u);

	const float ScreenRingRadius = RingRadius * 1.3f;  

	FVector2 prev;
	bool bHasPrev = false;

	for (int s = 0; s <= Segments; ++s)
	{
		float theta = (float)s / Segments * 2.0f * PI;
		FVector worldPos = Origin
			+ u * (ScreenRingRadius * cosf(theta))
			+ v * (ScreenRingRadius * sinf(theta));

		FVector2 screenPos = WorldToScreen(worldPos, ViewProj, ScreenW, ScreenH);

		if (bHasPrev)
		{
			float Dist = DistanceToSegment(MousePos, prev, screenPos);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = AXIS_SCREEN;
			}
		}

		prev = screenPos;
		bHasPrev = true;
	}

	return Best;
}

// LocalLightComponet 가져오기 (Rect, Point, Spot 등 Radius를 파라미터로 것들)
ULocalLightComponent* FGizmo::GetTargetLight() const
{
    if (!Target) return nullptr;
    ALightActorBase* Actor = Cast<ALightActorBase>(Target->GetOwner());
    return Actor ? Cast<ULocalLightComponent>(Actor->GetLightComponent()) : nullptr;
}

// 조명이면 radius만 바꾸고 true를 반환
bool FGizmo::ApplyScaleToLight(float Factor)
{
    ULocalLightComponent* Light = GetTargetLight();
    if (!Light) return false;
    Light->SetAttenuationRadius(DragStartRadius * Factor);
    return true;
}

// 시작 Transform·교차점을 저장하고 조작 축의 드래그 평면을 정한다.
void FGizmo::BeginDrag(int Axis, const FRay& MouseRay, const FVector2& MousePos)
{
	DragStartMousePos = MousePos;
	DraggingAxis = Axis;
	DragStartLocation = GetLocation();
	DragStartRenderLocation = GetRenderLocation();    
	DragStartRotation = GetRotation();
	DragStartScale = GetScale();

	// Local LightComponent면 Radius 가져와서 저장해놓기
	if (ULocalLightComponent* Light = GetTargetLight())
    	DragStartRadius = Light->GetAttenuationRadius();

	DragAxisDirection = GetAxisDirection(Axis);
	FVector axis = DragAxisDirection;

	if (Axis == AXIS_SCREEN)
	{
		if (Mode == EGizmoMode::Rotation)
		{
			DragAxisDirection = (GetRenderLocation() - GetCameraLocation()).Normalized();
			DragPlaneNormal = DragAxisDirection;
		}
		else
		{
			DragPlaneNormal = -MouseRay.Direction;
		}
	}
	else if (Mode == EGizmoMode::Rotation)
	{
		FVector ToCamera = (GetCameraLocation() - GetRenderLocation()).Normalized();
		if (DragAxisDirection.Dot(ToCamera) < 0.0f)
			DragAxisDirection = -DragAxisDirection;

		DragPlaneNormal = DragAxisDirection;
	}
	else
	{
		FVector ToCamera = -MouseRay.Direction;
		FVector Ortho = axis.Cross(ToCamera);
		DragPlaneNormal = Ortho.Cross(axis).Normalized();
	}

	float t;
	if (RayIntersectsPlane(MouseRay, DragStartRenderLocation, DragPlaneNormal, t))
		DragStartPoint = MouseRay.Origin + MouseRay.Direction * t;
	else
		DragStartPoint = DragStartRenderLocation;

	if (Mode == EGizmoMode::Rotation)
		DragStartAngle = ComputeAngleOnPlane(DragStartPoint, Axis);
}

// 평면 교차점 변화를 이동·회전·크기에 반영한다.
void FGizmo::UpdateDrag(const FRay& MouseRay, const FVector2& MousePos)
{
	float t;
	if (!RayIntersectsPlane(MouseRay, DragStartRenderLocation, DragPlaneNormal, t))
		return;

	FVector current = MouseRay.Origin + MouseRay.Direction * t;

	if (DraggingAxis == AXIS_SCREEN && Mode != EGizmoMode::Rotation)
	{
		if (Mode == EGizmoMode::Location)
			Target->SetRelativeLocation(DragStartLocation + (current - DragStartPoint));
		else
		{
			float dx = MousePos.X - DragStartMousePos.X;
			float dy = DragStartMousePos.Y - MousePos.Y;   // 화면 Y는 아래가 +
			float scaleDelta = (dx + dy) * 0.005f;

			float factor = 1.0f + scaleDelta;
			if (factor < 0.01f) factor = 0.01f;

			if (!ApplyScaleToLight(factor)) 	// 조명이면 여기서 radius만 변경하고 return
				Target->SetRelativeScale3D(DragStartScale * factor);
		}
		return;
	}

	if (Mode == EGizmoMode::Rotation)
	{
		float currentAngle = ComputeAngleOnPlane(current, DraggingAxis);
		float deltaAngle = currentAngle - DragStartAngle;

		FQuat delta = FQuat::MakeFromAxisAngle(DragAxisDirection, deltaAngle);
		FQuat start = DragStartRotation.Quaternion();

		FQuat result = delta * start;

		Target->SetRelativeRotation(result.ToFRotator());
		return;
	}

	FVector delta = current - DragStartPoint;
	float amount = delta.Dot(DragAxisDirection);

	if (Mode == EGizmoMode::Location)
		Target->SetRelativeLocation(DragStartLocation + DragAxisDirection * amount);
	else   // Scale
	{
		float factor = 1.0f + amount;
		if (factor < 0.01f) factor = 0.01f;

		// 조명이면 여기서 radius만 반영하고 return
		if (ApplyScaleToLight(factor)) return;

		FVector NewScale = DragStartScale;
		if (DraggingAxis == 0)      NewScale.X *= factor;
		else if (DraggingAxis == 1) NewScale.Y *= factor;
		else                        NewScale.Z *= factor;

		Target->SetRelativeScale3D(NewScale);
	}
}

// 드래그 축을 비워 조작을 종료한다.
void FGizmo::EndDrag()
{
	DraggingAxis = -1;
}

// 평면 기저에 점을 투영하고 atan2로 회전각을 구한다.
float FGizmo::ComputeAngleOnPlane(const FVector& Point, int Axis) const
{
	FVector u, v;

	if (DraggingAxis >= 0)
	{
		FVector Ref = (fabsf(DragAxisDirection.Z) > 0.9f) ? FVector(1, 0, 0) : FVector(0, 0, 1);
		u = Ref.Cross(DragAxisDirection).Normalized();
		v = DragAxisDirection.Cross(u);
	}
	else if (Axis == AXIS_SCREEN)
	{
		FVector Forward = (GetRenderLocation() - GetCameraLocation()).Normalized();
		u = FVector(0, 0, 1).Cross(Forward).Normalized();
		v = Forward.Cross(u);
	}
	else
	{
		u = AxisDirs[(Axis + 1) % 3];
		v = AxisDirs[(Axis + 2) % 3];
	}

	FVector local = Point - DragStartRenderLocation;
	return atan2f(local.Dot(v), local.Dot(u));
}

// 월드 축 또는 로컬 회전으로 변환한 정규화 축을 반환한다.
FVector FGizmo::GetAxisDirection(int Axis) const
{
	if (Axis < 0 || Axis > 2) return FVector(0, 0, 0);
	bool bUseLocal = (Space == EGizmoSpace::Local) || (Mode == EGizmoMode::Scale);

	if (bUseLocal && Target)
	{
		FMatrix rot = Target->GetRelativeRotation().Quaternion().ToFMatrix();
		FVector4 v = rot.TransformVector(AxisDirs[Axis]);
		return FVector(v.X, v.Y, v.Z).Normalized();
	}
	return AxisDirs[Axis];
}

// 입력 View 카메라 조건으로 Gizmo 표시 위치를 구한다.
FVector FGizmo::GetRenderLocation() const
{
	return GetRenderLocationForView(ViewCameraLocation, bViewCameraOrthographic);
}

// 직교에서는 대상 위치, 원근에서는 카메라 앞 일정 거리로 표시 위치를 정한다.
FVector FGizmo::GetRenderLocationForView(const FVector& CameraLocation, const bool bCameraOrthographic) const
{
	if (!Target) return FVector(0, 0, 0);

	FVector PivotLocation = Target->GetWorldLocation();

	// bounds 합집합 중앙에 기즈모 표시
	/*if (bUseActorBounds)
	{
		FBox Bounds;
		if (Target->GetOwner()->TryGetActorBounds(Bounds))
		{
			PivotLocation = (Bounds.Min + Bounds.Max) * 0.5f;
			
		}
	}*/

	if (bCameraOrthographic) return PivotLocation;

	return (PivotLocation - CameraLocation).Normalized() * 10.0f + CameraLocation;

}

// 현재 입력 View의 카메라 위치를 반환한다.
FVector FGizmo::GetCameraLocation() const
{
	return ViewCameraLocation;
}
