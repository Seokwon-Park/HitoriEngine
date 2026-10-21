#pragma once

#include "Core/Types.h"

class FLightSceneProxy;
class FScene;

// 라이트의 렌더러 쪽 상태. Proxy는 컴포넌트 값의 복사본이고, 이 구조체는 렌더러가 관리한다.
struct FLightSceneInfo
{
	FLightSceneInfo() = default;
	// FScene::Lights 안의 고정 인덱스. 다른 라이트가 지워져도 바뀌지 않는다.
	int32 Id = -1;
	FLightSceneProxy* Proxy = nullptr;
	bool bVisible = true;
	FScene* Scene = nullptr;

	void SetDynamicShadowMapChannel(int32 NewChannel) { DynamicShadowMapChannel = NewChannel; };
	int32 GetDynamicShadowMapChannel() const { return DynamicShadowMapChannel; };
protected:
	int32 DynamicShadowMapChannel = -1;
};
