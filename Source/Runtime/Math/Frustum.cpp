#include "EnginePCH.h"
#include "Frustum.h"

static FPlane NormalizePlane(const FPlane Plane)
{
    const FVector& N = Plane.Normal;
    const float Length = sqrtf(N.X * N.X + N.Y * N.Y + N.Z * N.Z);
    assert(Length > 1.0e-6f);

    const float InvLength = 1.0f / Length;
    return {
        { N.X * InvLength, N.Y * InvLength, N.Z * InvLength },
        Plane.Distance * InvLength
    };
}

FFrustumPlanes ExtractFrustumPlanes(const FMatrix& Matrix)
{
    auto MakePlane = [&Matrix](const int ColumnA, const float ScaleA, const int ColumnB, const float ScaleB)
        {
            return NormalizePlane({
                { ScaleA * Matrix.M[0][ColumnA] + ScaleB * Matrix.M[0][ColumnB], ScaleA * Matrix.M[1][ColumnA] + ScaleB * Matrix.M[1][ColumnB], ScaleA * Matrix.M[2][ColumnA] + ScaleB * Matrix.M[2][ColumnB] },
                ScaleA * Matrix.M[3][ColumnA] + ScaleB * Matrix.M[3][ColumnB] });
        };
    FFrustumPlanes Result{};
    Result.Planes[0] = MakePlane(3, 1.0f, 0, 1.0f);
    Result.Planes[1] = MakePlane(3, 1.0f, 0, -1.0f);
    Result.Planes[2] = MakePlane(3, 1.0f, 1, 1.0f);
    Result.Planes[3] = MakePlane(3, 1.0f, 1, -1.0f);
    Result.Planes[4] = NormalizePlane({ { Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2] }, Matrix.M[3][2] });
    Result.Planes[5] = MakePlane(3, 1.0f, 2, -1.0f);
    return Result;
}

bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum)
{
    FVectorRegister Boundreg = VectorSIMD::LoadFloat3(&Bounds.Extent.X);
    FVectorRegister Centerreg = VectorSIMD::LoadFloat3(&Bounds.Center.X);
    for (const FPlane& Plane : Frustum.Planes)
    {
        FVectorRegister Planereg = VectorSIMD::LoadFloat3(&Plane.Normal.X);
        FVectorRegister absPlanereg = VectorSIMD::Abs(Planereg);

        const float Radius = VectorSIMD::Dot(absPlanereg, Boundreg);
        const float CenterDist = VectorSIMD::Dot(Planereg, Centerreg);

        if (CenterDist + Plane.Distance + Radius < 0.0f)
        {
            return false;
        }
    }
    return true;
}


FAABB MakeWorldBounds(const FBox& Value)
{
    const FVector Center = (Value.Min + Value.Max) * 0.5f;
    const FVector Extent = (Value.Max - Value.Min) * 0.5f;
    return { Center, Extent };
}
