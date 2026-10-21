#pragma once

#include "Components/LightComponent.h"
#include "SceneManagement.h"
#include "Render/LightRenderData.h"
#include "Render/LightSceneInfo.h"
#include "Render/SceneView.h"

class UDirectionalLightComponent;

struct FSceneView;
// MSVC 최적화
// 가상함수 테이블과 패딩 방지
class FLightSceneProxyFixLayout
{
public:
	virtual ~FLightSceneProxyFixLayout() = default;
};


class FLightSceneProxy : public FLightSceneProxyFixLayout
{
public:
	FLightSceneProxy(const ULightComponent* InLightComponent)
		: LightComponent(InLightComponent) {
		if (!InLightComponent) return;
		bCastDynamicShadow = InLightComponent->CastsDynamicShadow();
		ShadowBias = InLightComponent->ShadowBias;
		ShadowSlopeBias = InLightComponent->ShadowSlopeBias;
		ShadowResolutionScale = InLightComponent->ShadowResolutionScale;
		ShadowSharpen = InLightComponent->ShadowSharpen;
	}
	virtual ~FLightSceneProxy() = default;

	virtual void GetLightShaderParameters(FLightRenderParameters& OutParameters) const = 0;
	virtual float GetLightSourceAngle() const { return 0.0f; }

	inline const ULightComponent* GetLightComponent() const { return LightComponent; }
	inline bool IsVisible() const { return LightComponent ? LightComponent->IsVisible() : false; }
	inline bool IsEnabled() const { return LightComponent && LightComponent->GetIntensity() > 0.0f; }
	inline bool IsShown(bool bGameView) const { return LightComponent ? LightComponent->IsShown(bGameView) : false; }

	inline bool CastsDynamicShadow() const { return bCastDynamicShadow; }
	inline float GetUserShadowBias() const { return ShadowBias; }
	inline float GetUserShadowSlopeBias() const { return ShadowSlopeBias; }
	inline float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	inline float GetShadowSharpen() const { return ShadowSharpen; }

	//각 Light Scene Proxy는 WholdSceneProjectedShadowInitializer를 채운다
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
	{
		return false;
	}

	virtual uint32 GetNumViewDependentWholeSceneShadows(const FSceneView& View/*, bool bPrecomputedLightingIsValid*/ ) const { return 0; }
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(
		const FSceneView& View,
		int32 InCascadeIndex,
		//bool bPrecomputedLightingIsValid,
		FWholeSceneProjectedShadowInitializer& OutInitializer) const
	{
		return false;
	}

	virtual FSphere GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const { return FSphere(FVector::ZeroVector, 0); }

	FLightSceneInfo* GetLightSceneInfo() const { return LightSceneInfo; }

protected:
	const ULightComponent* LightComponent = nullptr;

	bool  bCastDynamicShadow = false;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowResolutionScale = 1.0f;
	float ShadowSharpen = 0.0f;
	FLightSceneInfo* LightSceneInfo = nullptr;

private:
	friend class FScene;
	FScene* Scene = nullptr;
	int32 PackedIndex = INDEX_NONE;
};

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	FDirectionalLightSceneProxy(const UDirectionalLightComponent* InComponent);

	void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	//virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const override;
	virtual uint32 GetNumViewDependentWholeSceneShadows(const FSceneView& View/*, bool bPrecomputedLightingIsValid*/) const override;
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(
		const FSceneView& View,
		int32 InCascadeIndex,
		//bool bPrecomputedLightingIsValid,
		FWholeSceneProjectedShadowInitializer& OutInitializer) const override;
	virtual FSphere GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const override;

	virtual float GetLightSourceAngle() const override { return LightSourceAngle; }
	float LightSourceAngle = 0.0f;   // 생성자에서 InComponent->LightSourceAngle 복사

	float GetSplitDistance(const FSceneView& View, uint32 SplitIndex) const;

	// 카메라에서 이 거리까지만 동적그림자를 그리겠다
	float  DynamicShadowDistanceMovableLight;
	// Cascade 개수
	uint32 DynamicShadowCascades;
	float  CascadeDistributionExponent;
	float CascadeTransitionFraction;
};

class FLocalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
};

class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;
	virtual void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const override;

};

class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
	using FPointLightSceneProxy::FPointLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneView& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const override;
};

class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
};

class FAmbientLightSceneProxy
{
public:
	bool IsEnabled() const { return false; }
	bool IsShown(bool) const { return false; }
	const FAmbientLightRenderData& GetRenderData() const { return RenderData; }

private:
	FAmbientLightRenderData RenderData{};
};

class FSkyLightSceneProxy
{
public:
	bool IsEnabled() const { return false; }
	bool IsShown(bool) const { return false; }
	const FSkyLightRenderData& GetRenderData() const { return RenderData; }

private:
	FSkyLightRenderData RenderData{};
};
