#include "EnginePCH.h"
#include "Ray.h"

#include <algorithm>
#include "Math/EngineMath.h"
#include "Render/StaticMeshData.h"


FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix)
{
    // ray를 로컬공간으로
    FMatrix invWorld = WorldMatrix.Inverse();
    FRay LocalRay{};
    LocalRay.Origin = invWorld.TransformPosition(WorldRay.Origin);
    LocalRay.Direction = invWorld.TransformVector(WorldRay.Direction);

    return LocalRay;
}

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT)
{
	FPreparedRay PreparedRay(Ray);
    return RayIntersectsAABB(PreparedRay, BoxMin, BoxMax, OutT);
}

bool RayIntersectsAABB(const FPreparedRay& PreparedRay, const FVector& BoxMin, const FVector& BoxMax, float& OutT)
{
    FVectorRegister Boxminreg = VectorSIMD::LoadFloat3(&BoxMin.X);
    FVectorRegister Boxmaxreg = VectorSIMD::LoadFloat3(&BoxMax.X);

    FVectorRegister tX1reg = VectorSIMD::Mul(VectorSIMD::Sub(Boxminreg, PreparedRay.Origin), PreparedRay.InvDirection);
    FVectorRegister tX2reg = VectorSIMD::Mul(VectorSIMD::Sub(Boxmaxreg, PreparedRay.Origin), PreparedRay.InvDirection);

    FVectorRegister tminreg = VectorSIMD::Min(tX1reg, tX2reg);
    FVectorRegister tmaxreg = VectorSIMD::Max(tX1reg, tX2reg);

    FVectorRegister maxXY = VectorSIMD::Max(tminreg, VectorSIMD::SplatY(tminreg));
    FVectorRegister maxXYZ = VectorSIMD::Max(maxXY, VectorSIMD::SplatZ(tminreg));
    float tEnter = _mm_cvtss_f32(maxXYZ);

    FVectorRegister minXY = VectorSIMD::Min(tmaxreg, VectorSIMD::SplatY(tmaxreg));
    FVectorRegister minXYZ = VectorSIMD::Min(minXY, VectorSIMD::SplatZ(tmaxreg));
    float tExit = _mm_cvtss_f32(minXYZ);

    if (tEnter > tExit)
    {   // 충돌 안함
        return false;
    }

    if (tExit < 0.0f)
    {   // 박스가 Ray 뒤에 있을 경우
        return false;
    }

    // 광선이 내부라면 tEnter는 음수. 
    OutT = fmax(0.0f, tEnter);
    return true;
}

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT)
{
    return RayIntersectsTriangleEdges(Ray, v1, v2 - v1, v3 - v1, OutT);
}

bool RayIntersectsTriangleEdges(const FRay& Ray, const FVector& V0, const FVector& Edge1, const FVector& Edge2, float& OutT)
{
    constexpr float epsilon = 1e-5f;

    FVectorRegister edge1 = VectorSIMD::LoadFloat3(&Edge1.X);
    FVectorRegister edge2 = VectorSIMD::LoadFloat3(&Edge2.X);

    FVectorRegister RayVector = VectorSIMD::LoadFloat3(&Ray.Direction.X);
    const FVectorRegister rayCrossVec = VectorSIMD::Cross3(RayVector, edge2);
    float det = VectorSIMD::Dot(rayCrossVec, edge1);
    if (fabs(det) < epsilon)
    {   // 내적의 결과가 0에 가까우면 180도. 평행한 관계
        return false;
    }

    float invDet = 1.0f / det;
    // 수식: Ray.Origin - v1 = u * edge1 + v * edge2 - t * Ray.Direction
    // 1. u 구하기
    FVectorRegister s = VectorSIMD::Sub(VectorSIMD::LoadFloat3(&Ray.Origin.X), VectorSIMD::LoadFloat3(&V0.X));
    float u = invDet * VectorSIMD::Dot(s, rayCrossVec);

    if (-epsilon > u || epsilon < u - 1)
    {
        return false;
    }

    FVectorRegister sCrossE1 = VectorSIMD::Cross3(s, edge1);
    float v = invDet * VectorSIMD::Dot(RayVector, sCrossE1);

    if (-epsilon > v || epsilon < u + v - 1)
    {
        return false;
    }

    float t = invDet * VectorSIMD::Dot(edge2, sCrossE1);

    if (t > epsilon)
    {
        OutT = t;
        return true;
    }

    return false;
}

// Mesh AABB를 통과한 Ray에 삼각형 교차를 적용해 가장 가까운 거리만 반환한다.
bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& InOutNearestT)
{
    if (!Mesh.TriangleBVH)
    {
	    FBox Box = Mesh.AABB;
	    float BoxT{};

        if (!RayIntersectsAABB(LocalRay, Box.Min, Box.Max, BoxT) ||
			BoxT >= InOutNearestT)
			return false;
    }

    bool bHit = false;
    float NearestT = InOutNearestT;

    const FPreparedRay PreparedRay(LocalRay);

    if (Mesh.TriangleBVH)
    {
        bHit = Mesh.TriangleBVH->TraceClosest(
            [&](const FBox& Bounds, float& OutEnterT)
            {
                return RayIntersectsAABB(PreparedRay, Bounds.Min, Bounds.Max, OutEnterT);
            },
            [&](const FMeshTriangleElement& Element, float& OutNearestT)
            {
                float T = InOutNearestT;
                if (RayIntersectsTriangleEdges(
                    LocalRay,
                    Element.V0,
                    Element.Edge1,
                    Element.Edge2,
                    T) &&
                    T < OutNearestT)
                {
                    OutNearestT = T;
                    return true;
                }

                return false;
            },
            NearestT);
    }
    else
    {
        for (uint32 i = 0; i + 2 < Mesh.Indices.Num(); i += 3)
        {
            FVector vertices[3]{};	// 3 vertex
            for (uint32 j = 0; j < 3; ++j)
            {
                uint32 index = Mesh.Indices[i + j];

                vertices[j].X = Mesh.Vertices[index].Position.X;
                vertices[j].Y = Mesh.Vertices[index].Position.Y;
                vertices[j].Z = Mesh.Vertices[index].Position.Z;
            }

            float T = FLT_MAX;
            if (RayIntersectsTriangle(LocalRay, vertices[0], vertices[1], vertices[2], T) && T < NearestT)
            {
                NearestT = T;
                bHit = true;
            }
        }
    }

    if (bHit) InOutNearestT = NearestT;
    return bHit;
}

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
    FVector4 clip = FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f) * ViewProj;

    if (clip.W < 0.0001f)
        return FVector2(-FLT_MAX, -FLT_MAX);

    float ndcX = clip.X / clip.W;
    float ndcY = clip.Y / clip.W;

    FVector2 result;
    result.X = (ndcX * 0.5f + 0.5f) * ScreenW;
    result.Y = (1.0f - (ndcY * 0.5f + 0.5f)) * ScreenH;   // Y 뒤집기
    return result;
}

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B)
{
    FVector2 seg = B - A;
    float segLenSq = seg.X * seg.X + seg.Y * seg.Y;

    if (segLenSq < 1e-6f)
    {
        FVector2 d = P - A;
        return sqrtf(d.X * d.X + d.Y * d.Y);
    }

    FVector2 toP = P - A;
    float t = (toP.X * seg.X + toP.Y * seg.Y) / segLenSq;

    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

    FVector2 closest = A + seg * t;
    FVector2 diff = P - closest;
    return sqrtf(diff.X * diff.X + diff.Y * diff.Y);
}

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT)
{
    float denom = Ray.Direction.Dot(PlaneNormal);

    if (fabsf(denom) < 1e-6f)
        return false;

    OutT = (PlanePoint - Ray.Origin).Dot(PlaneNormal) / denom;

    return OutT >= 0.0f;
}
