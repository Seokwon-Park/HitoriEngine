#pragma once

#include "Render/Renderer.h"
#include "Render/PipelineState.h"
#include "Editor/Gizmo/Gizmo.h"

class UCameraComponent;

struct FGizmoData
{
	// Gizmo 한 Draw에 필요한 월드·ViewProjection 행렬과 축 색상을 담는다.
	FMatrix World;
	FMatrix ViewProj;
	FVector4 Color;
};

struct FAxisData
{
	// 축별 기본 회전과 표시 색상을 담는다.
	FRotator Rotator;
	FVector4 Color;
};

class FGizmoRenderer
{
public:
	FGizmoRenderer();
	~FGizmoRenderer() = default;

	bool Init(FRenderer* InRenderer);
	// View별 카메라 조건으로 계산한 위치에 동일한 Gizmo 축 Mesh를 그린다.
	void OnRender(const FGizmo& Gizmo, const FMatrix& ViewProj, const FVector& CameraLocation, bool bCameraOrthographic);

private:
	void DrawMesh(UStaticMesh* Mesh, const FGizmoData& Data);

	FRenderer* Renderer;

	FTransform Transform;
	
	TArray<FAxisData> AxisDataArray;

	FShaderProgram* Shader;
	FPipelineState PipelineState;
	UStaticMesh* LocationMesh;
	UStaticMesh* RotationMesh;
	UStaticMesh* ScaleMesh;
	UStaticMesh* SphereMesh;
	TUniquePtr<FConstantBuffer> CB;

	//EGizmoMode Mode = EGizmoMode::Scale;
};
