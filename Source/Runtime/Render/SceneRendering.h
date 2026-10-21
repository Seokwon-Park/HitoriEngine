#pragma once

#include "Containers/Array.h"
#include "Render/ShadowRendering.h"

class FVisibleLightInfo
{
public:
	// 이 라이트의 그림자 전부
	TArray<FProjectedShadowInfo*> AllProjectedShadows;
	// 이번 뷰에서 실제로 적용할 그림자
	TArray<FProjectedShadowInfo*> ShadowsToProject;
};
