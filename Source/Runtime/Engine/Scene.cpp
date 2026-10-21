#include "EnginePCH.h"
#include "Scene.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = PrimitiveSceneProxies.Num();
	Component->SceneProxy = Proxy;
	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	PrimitiveSceneProxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	PrimitiveSceneProxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(PrimitiveSceneProxies.Num()))
		PrimitiveSceneProxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : PrimitiveSceneProxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
			Component->SceneProxy = nullptr;
		delete Proxy;
	}
	PrimitiveSceneProxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::AddLight(ULightComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FLightSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy)
	{
		if (USkyLightComponent* Sky = Cast<USkyLightComponent>(Component))
			if (SkyLights.Find(Sky) == INDEX_NONE) SkyLights.Add(Sky);
		return;
	}

	Proxy->Scene = this;
	Component->SceneProxy = Proxy;

	if (Component->GetLightType() == ELightType::Directional)
	{
		Proxy->PackedIndex = DirectionalLightProxies.Num();
		DirectionalLightProxies.Add(static_cast<FDirectionalLightSceneProxy*>(Proxy));
	}
	else
	{
		Proxy->PackedIndex = LocalLightProxies.Num();
		LocalLightProxies.Add(static_cast<FLocalLightSceneProxy*>(Proxy));
	}

	// PackedIndex는 RemoveAtSwap 때마다 바뀌므로, Id는 Lights의 빈 칸을 재사용해 고정한다.
	FLightSceneInfo* Info = new FLightSceneInfo();
	Info->Proxy = Proxy;
	Info->bVisible = Proxy->IsVisible();
	Info->Scene = this;
	Info->Id = Lights.Find(nullptr);
	if (Info->Id == INDEX_NONE)
	{
		Info->Id = Lights.Num();
		Lights.Add(Info);
	}
	else
	{
		Lights[Info->Id] = Info;
	}

	if (Proxy->CastsDynamicShadow())
	{
		bool bUsed[4] = {};
		for (const FLightSceneInfo* Other : Lights)
		{
			if (!Other || Other == Info) continue;
			int32 DynamicShadowChannel = Other->GetDynamicShadowMapChannel();
			if (Other && Other != Info && DynamicShadowChannel  >= 0)
				bUsed[DynamicShadowChannel] = true;
		}
		for (int32 Channel = 0; Channel < 4; ++Channel)
			if (!bUsed[Channel]) 
			{
				Info->SetDynamicShadowMapChannel(Channel); 
				break; 
			}
	}
	Proxy->LightSceneInfo = Info;
}

void FScene::RemoveLight(ULightComponent* Component)
{
	if (!Component) return;

	FLightSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy)
	{
		if (USkyLightComponent* Sky = Cast<USkyLightComponent>(Component))
		{
			const int32 Found = SkyLights.Find(Sky);
			if (Found != INDEX_NONE) SkyLights.RemoveAtSwap(Found);
		}
		return;
	}

	const int32 Index = Proxy->PackedIndex;
	if (Component->GetLightType() == ELightType::Directional)
	{
		DirectionalLightProxies.RemoveAtSwap(Index);
		if (Index < DirectionalLightProxies.Num())
			DirectionalLightProxies[Index]->PackedIndex = Index;
	}
	else
	{
		LocalLightProxies.RemoveAtSwap(Index);
		if (Index < LocalLightProxies.Num())
			LocalLightProxies[Index]->PackedIndex = Index;
	}

	if (FLightSceneInfo* Info = Proxy->LightSceneInfo)
	{
		if (Info->Id >= 0 && Info->Id < Lights.Num()) Lights[Info->Id] = nullptr;
		delete Info;
	}

	Component->SceneProxy = nullptr;
	delete Proxy;
}

void FScene::RemoveAllLights()
{
	for (FLightSceneInfo* Info : Lights) delete Info;
	Lights.Reset();
	for (FLightSceneProxy* Proxy : DirectionalLightProxies) delete Proxy;
	for (FLightSceneProxy* Proxy : LocalLightProxies)       delete Proxy;
	DirectionalLightProxies.Reset();
	LocalLightProxies.Reset();
	SkyLights.Reset();
	SkyLightEnvironments.clear();
}

FAmbientLightRenderData FScene::GetSkyAmbientLight(bool bGameView) const
{
	for (const USkyLightComponent* Sky : SkyLights)
	{
		if (!Sky || !Sky->IsShown(bGameView)) continue;
		const FLinearColor Color(Sky->GetLightColor());
		return { FVector4(Color.R, Color.G, Color.B, Sky->GetIntensity()) };
	}
	return {};
}

void FScene::PrepareSkyLight(const FSkyLightRenderData& SkyLight)
{
	if (SkyLight.SourcePath.empty() || SkyLightEnvironments.contains(SkyLight.SourcePath)) return;
	auto Environment = MakeUnique<FSkyLightEnvironment>();
	Environment->Prepare(SkyLight.SourcePath);
	SkyLightEnvironments.emplace(SkyLight.SourcePath, std::move(Environment));
}

FSkyLightEnvironment* FScene::FindPendingSkyLightEnvironment(const FString& SourcePath)
{
	const auto It = SkyLightEnvironments.find(SourcePath);
	return It != SkyLightEnvironments.end() && It->second->IsPrepared() && !It->second->IsValid()
		? It->second.get() : nullptr;
}

const FSkyLightEnvironment* FScene::FindSkyLightEnvironment(const FString& SourcePath) const
{
	const auto It = SkyLightEnvironments.find(SourcePath);
	return It != SkyLightEnvironments.end() && It->second->IsValid() ? It->second.get() : nullptr;
}



void FScene::UpdateAllTransforms()
{
	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->IsVisible() ? 1 : 0;
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}

	//const int32 Count = Proxies.Num();
	//for (int32 i = 0; i < Count; ++i)
	//{
	//	FPrimitiveSceneProxy* Proxy = Proxies[i];
	//	Proxy->UpdateTransform();

	//	const FAABB& Bounds = Proxy->GetBounds();
	//	if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
	//	{
	//		bBoundsChanged = true;
	//	}

	//	PrimitiveBounds[i] = Proxy->GetBounds();
	//	PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	//}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();


	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(PrimitiveSceneProxies.Num());
	for (FPrimitiveSceneProxy* Proxy : PrimitiveSceneProxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}
