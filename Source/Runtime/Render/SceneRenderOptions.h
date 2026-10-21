#pragma once

#include "Render/RenderEnums.h"

// 뷰 하나의 씬 렌더링 옵션. 에디터와 게임 뷰포트가 같은 설정을 전달한다.
struct FSceneRenderOptions
{
	bool bDrawPrimitives = true;
	bool bEnableFog = true;
	bool bEnableFXAA = false;
	ELightCullingMode LightCullingMode = ELightCullingMode::Tiled;
	bool bShowSceneDepth = false;
	bool bShowWorldNormal = false;
	bool bShowLightCulling = false;
	float SceneDepthRange = 1.0f;

	ERenderingPath RenderingPath = ERenderingPath::Deferred;
	ELightingModel LightingModel = ELightingModel::Gouraud;
};
