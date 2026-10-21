#include "EnginePCH.h"
#include "Editor/Gizmo/GizmoRenderer.h"

#include "Render/GeometryGenerator.h"
#include "Camera/CameraComponent.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Asset/AssetManager.h"


// Gizmo 축 렌더링의 초기 상태를 구성한다.
FGizmoRenderer::FGizmoRenderer()
{
}

// 축 Mesh·Shader·상수 버퍼와 축별 색상·회전을 준비한다.
bool FGizmoRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	
	LocationMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Arrow");
	RotationMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Ring");
	ScaleMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ScaleBar");
	SphereMesh = UAssetManager::GetAssetByPath<UStaticMesh>("GizmoSphere");

	CB = RenderCommand::CreateConstantBuffer(sizeof(FGizmoData));

	AxisDataArray.Add({ FRotator(90.0f, 0.0f, 0.0f) ,FVector4(1.0f, 0.0f, 0.0f, 1.0f) });
	AxisDataArray.Add({ FRotator(0.0f, 0.0f, -90.0f) ,FVector4(0.0f, 1.0f, 0.0f, 1.0f) });
	AxisDataArray.Add({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.4f, 1.0f, 1.0f) });

	AxisDataArray.Add({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.Add({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.Add({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.Add({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(1.0f, 1.0f, 1.0f, 1.0f) }); // ScreenAxis

	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/GizmoShader.hlsl");

	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return false;
}

// 렌더 View의 카메라로 표시 위치를 구해 Gizmo Mesh를 그린다.
void FGizmoRenderer::OnRender(
	const FGizmo& Gizmo,
	const FMatrix& ViewProj,
	const FVector& CameraLocation,
	const bool bCameraOrthographic)
{
	if (!Gizmo.GetTarget())
		return;

	UStaticMesh* AxisMesh = nullptr;
	switch (Gizmo.GetMode())
	{
	case EGizmoMode::Location: AxisMesh = LocationMesh; break;
	case EGizmoMode::Rotation: AxisMesh = RotationMesh; break;
	case EGizmoMode::Scale:    AxisMesh = ScaleMesh;    break;
	default: return;
	}

	RenderCommand::BindPipelineState(PipelineState);

	// const FVector GizmoLocation = Gizmo.GetLocation();
	const int HoveredAxis = Gizmo.GetHoveredAxis();

	const FVector GizmoLocation = Gizmo.GetRenderLocationForView(CameraLocation, bCameraOrthographic);
	// 축 3개
	RenderCommand::BindMesh(AxisMesh);
	Transform.Location = GizmoLocation;

	for (int i = 0; i < 3; ++i)
	{
		FMatrix World;

		if (Gizmo.GetMode() == EGizmoMode::Scale || Gizmo.GetSpace() == EGizmoSpace::Local)
		{
			FMatrix AxisRot = AxisDataArray[i].Rotator.Quaternion().ToFMatrix();
			FMatrix ObjRot = Gizmo.GetRotation().Quaternion().ToFMatrix();
			FMatrix Trans = FMatrix::MakeTranslation(GizmoLocation);
			World = AxisRot * ObjRot * Trans;
		}
		else
		{
			Transform.Location = GizmoLocation;
			Transform.Rotation = AxisDataArray[i].Rotator;
			World = Transform.GetLocalMatrix();
		}

		FGizmoData Data{};
		Data.World = World;
		Data.ViewProj = ViewProj;
		Data.Color = (i == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[i].Color;

		DrawMesh(AxisMesh, Data);
	}

	if (Gizmo.GetMode() != EGizmoMode::Rotation)
	{
		// 중앙 구
		Transform.Rotation = FRotator(0.0f, 0.0f, 0.0f);

		FGizmoData SphereData{};
		SphereData.World = Transform.GetLocalMatrix();
		SphereData.ViewProj = ViewProj;
		SphereData.Color = (6 == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[6].Color;

		DrawMesh(SphereMesh, SphereData);
	}
	else
	{
		FVector Forward = (GizmoLocation - CameraLocation).Normalized();
		FVector Right = FVector(0, 0, 1).Cross(Forward).Normalized();
		FVector Up = Forward.Cross(Right);

		FMatrix Rot = FMatrix::Identity;
		Rot.M[0][0] = Right.X;   Rot.M[0][1] = Right.Y;   Rot.M[0][2] = Right.Z;
		Rot.M[1][0] = Up.X;      Rot.M[1][1] = Up.Y;      Rot.M[1][2] = Up.Z;
		Rot.M[2][0] = Forward.X; Rot.M[2][1] = Forward.Y; Rot.M[2][2] = Forward.Z;

		FMatrix Scale = FMatrix::Identity;
		Scale.M[0][0] = Scale.M[1][1] = Scale.M[2][2] = 1.3f;

		FMatrix Trans = FMatrix::MakeTranslation(GizmoLocation);

		FGizmoData Data{};
		Data.World = (Scale * Rot * Trans);
		Data.ViewProj = ViewProj;
		Data.Color = (6 == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[6].Color;


		DrawMesh(RotationMesh, Data);
	}
}

// 행렬·색상 상수를 바인딩해 지정 Gizmo Mesh를 그린다.
void FGizmoRenderer::DrawMesh(UStaticMesh* Mesh, const FGizmoData& Data)
{
	RenderCommand::BindMesh(Mesh);
	RenderCommand::UpdateBufferData(CB.get(), &Data, sizeof(FGizmoData));
	RenderCommand::BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::DrawIndexed(Mesh->IndexBuffer->GetIndexCount());
}
