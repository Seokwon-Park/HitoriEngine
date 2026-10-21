#pragma once

#include <d3d11.h>
#include "Render/Renderer.h"
#include "Render/RenderingInfo.h"
#include "Render/PipelineState.h"

struct FPSGridData
{
	// 원근 Grid Shader가 월드 평면을 복원하고 거리별 간격을 계산할 상수를 담는다.
	FMatrix invViewProj;
	FVector CameraPos;
	int32 CellSize;
	float SubCellSize;
	int32 GridPlaneType;
	float Padding[2];
};

enum class EGridPlane : int32
{
	// 직교 View가 표시할 월드 평면을 축 조합으로 구분한다.
	XY = 0,
	XZ = 1,
	YZ = 2,
};

// 선 Shader에서 사용하는 ViewProjection과 렌더 타깃 픽셀 크기를 담는다.
struct FBatchGridData
{
    FMatrix ViewProjection;
    FVector2 ViewportSize;
    float Padding[2];
    // 원근 축에만 적용할 Grid 거리 페이드의 중심과 반경을 담는다.
    FVector4 FadeOriginAndRadius;
};

// 월드 선의 양 끝점, 꼭짓점 선택·방향·반두께, 색상을 담는다.
struct FGridLineVertex
{
    FVector Start;
    FVector End;
    FVector Shape;
    FVector4 Color;
};


struct FEditorSettings;

class FGridRenderer
{
public:
	// CPU에서 재사용한 Grid 선 정점 배열을 해제한다.
	~FGridRenderer();

	// Grid·축 Shader와 정점·상수 버퍼 및 깊이·블렌드 상태를 준비한다.
	bool Init(FRenderer* InRenderer);
	// 기존 렌더 진입점 선언이며 현재 호출은 원근·직교 전용 함수를 사용한다.
	void OnRender(const FMatrix& ViewProj, const FVector& CameraPos);

	// 원근 Grid 앞뒤로 Z축을 나눠 합성하고 음수 Z는 낮은 불투명도로 그린다.
	void OnRenderPSGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings, const FViewportSettings& Viewport);
	// 직교 Grid와 축에 픽셀 두께·보간을 적용하고 평면 관통 축은 앞뒤로 나눠 합성한다.
	void OnRenderBatchGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FVector& CameraForward, EGridPlane Plane, float GridSpacing, bool bDrawAllWorldAxes, const FViewportSettings& Viewport);

private:
	// 투영하지 않은 월드 선과 두께를 재사용 버퍼에 담아 GPU에 전달한다.
	void AddWorldLine(const FVector& Start, const FVector& End, const FVector4& Color, float HalfWidth, uint32& VertexCount);
	// 원점에 고정된 월드 축을 추가하며 X·Y·Z 모두 음수 방향을 양수 방향보다 옅게 표시한다.
	void AddWorldAxes(EGridPlane Plane, bool bAllAxes, const FVector& AxisExtent, uint32& VertexCount);
	// VP와 픽셀 크기를 바인딩해 월드 선을 GPU에서 투영하고 깊이 검사로 그린다.
	void DrawWorldLines(uint32 VertexCount, const FMatrix& ViewProj, const FViewportSettings& Viewport, const FVector& FadeOrigin = FVector(0, 0, 0), float FadeRadius = 0.0f);
	FRenderer* Renderer;

	// Batch Grid
	FShaderProgram*			BatchGridShader;
	FPipelineState				BatchGridPipelineState;
	TUniquePtr<FVertexBuffer>	BatchGridVertexBuffer;
	TUniquePtr<FConstantBuffer>	BatchGridConstantBuffer;
	FGridLineVertex* BatchGridVertices = nullptr;
	uint32						MaxVertices = 0;

	// PS Grid
	FShaderProgram*			PSGridShader;
	FPipelineState									PSGridPipelineState;
	TUniquePtr<FConstantBuffer>						PSGridConstantBuffer;
	ComPtr<ID3D11RasterizerState>	RasterizerState;
};
