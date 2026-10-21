#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"

class ULightComponent : public ULightComponentBase
{
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	REFLECT_START(ULightComponent)
		PROPERTY(ShadowResolutionScale)
		PROPERTY(ShadowBias)
		PROPERTY(ShadowSlopeBias)
		PROPERTY(ShadowSharpen)
	REFLECT_END()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;

	virtual class FLightSceneProxy* CreateSceneProxy() const
	{
		return NULL;
	}
	FLightSceneProxy* SceneProxy = nullptr;

	void SetShadowBias(float NewValue);
	void SetShadowSlopeBias(float NewValue);
	bool CastsDynamicShadow() const; 

	virtual void MarkRenderStateDirty() override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// 섀도우 맵 해상도 배율
	float ShadowResolutionScale = 1.0f;
	// 고정 깊이 바이어스
	float ShadowBias = 0.5f;
	//기울기 비례 바이어스
	float ShadowSlopeBias = 0.5f;
	// PCF 등으로 필터링된 그림자 경계를 날카롭게
	float ShadowSharpen = 0.0f;
protected:
	virtual bool ShouldCreateRenderState() const override
	{ return true; }

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;
private:
};