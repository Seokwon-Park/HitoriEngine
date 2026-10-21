#pragma once

#include "Math/EngineMath.h"

struct FStaticMeshData;

struct FRay
{
	// 광선 시작위치
	FVector Origin;
	// 광선 방향
	FVector Direction;

	FVector PointAt(float t) const
	{
		return Origin + Direction * t;
	}
};

struct FPreparedRay
{
	FVectorRegister Origin;
	FVectorRegister InvDirection;

	explicit FPreparedRay(const FRay& Ray)
		: Origin(VectorSIMD::LoadFloat3(&Ray.Origin.X))
		, InvDirection(VectorSIMD::Reciprocal(VectorSIMD::LoadFloat3(&Ray.Direction.X)))
	{}
};

// 월드 레이를 로컬로. 방향은 정규화하지 않는다 (t가 월드 거리로 유지되도록)
FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix);

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT);
bool RayIntersectsAABB(const FPreparedRay& PreparedRay, const FVector& BoxMin, const FVector& BoxMax, float& OutT);

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT);

bool RayIntersectsTriangleEdges(const FRay& Ray, const FVector& V0, const FVector& Edge1, const FVector& Edge2, float& OutT);

bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& InOutNearestT);

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B);

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT);
