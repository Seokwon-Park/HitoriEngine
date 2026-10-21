#pragma once

#include "Vector.h"

struct FBoxSphereBounds
{
	FVector Origin;       // AABB와 구의 공통 중심
	FVector BoxExtent;    // AABB 반 크기 (half-extent)
	float SphereRadius; // 바운딩 구 반지름
	
};
