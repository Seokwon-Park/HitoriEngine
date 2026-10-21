#pragma once

#include "Box.h"

struct FPlane { FVector Normal; float Distance; };
struct FFrustumPlanes { FPlane Planes[6]; };
struct FAABB { FVector Center; FVector Extent; };

static FPlane NormalizePlane(const FPlane Plane);
FFrustumPlanes ExtractFrustumPlanes(const FMatrix& Matrix);
bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum);
FAABB MakeWorldBounds(const FBox& Value);

enum class EFrustumTest : uint8 { Outside, Intersect, Inside };

// 6개 평면 모두 검사해야 하는 초기 마스크. 비트 i가 켜져 있으면 평면 i를 아직 검사해야 한다.
inline constexpr uint32 FrustumAllPlanesMask = 0x3F;

// Min/Max 박스를 변환 없이 바로 판정한다. 박스가 완전히 안쪽에 있는 평면은 마스크에서 지워
// 자식 노드가 그 평면을 다시 검사하지 않게 한다. 마스크가 0이 되면 Inside다.
inline EFrustumTest ClassifyBoxInFrustum(const FBox& Box, const FFrustumPlanes& Frustum, uint32& InOutPlaneMask)
{
	const float CX = (Box.Min.X + Box.Max.X) * 0.5f;
	const float CY = (Box.Min.Y + Box.Max.Y) * 0.5f;
	const float CZ = (Box.Min.Z + Box.Max.Z) * 0.5f;
	const float EX = (Box.Max.X - Box.Min.X) * 0.5f;
	const float EY = (Box.Max.Y - Box.Min.Y) * 0.5f;
	const float EZ = (Box.Max.Z - Box.Min.Z) * 0.5f;

	for (uint32 PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
	{
		const uint32 Bit = 1u << PlaneIndex;
		if (!(InOutPlaneMask & Bit))
			continue;

		const FPlane& Plane = Frustum.Planes[PlaneIndex];
		const float Distance = Plane.Normal.X * CX + Plane.Normal.Y * CY + Plane.Normal.Z * CZ + Plane.Distance;
		const float Radius = std::abs(Plane.Normal.X) * EX + std::abs(Plane.Normal.Y) * EY + std::abs(Plane.Normal.Z) * EZ;

		if (Distance + Radius < 0.0f)
			return EFrustumTest::Outside;
		if (Distance - Radius >= 0.0f)
			InOutPlaneMask &= ~Bit;
	}

	return InOutPlaneMask == 0 ? EFrustumTest::Inside : EFrustumTest::Intersect;
}