#pragma once

#include "Math/Matrix.h"
#include "Math/Box.h"

class FSoftwareOcclusion
{
public:
	void Begin(const FMatrix& InViewProjection, uint32 InWidth, uint32 InHeight);
	//void RasterizeOccluder(const FOcculderMesh& Mesh)
};