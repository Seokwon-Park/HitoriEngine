#include "EnginePCH.h"
#include "Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Camera/CameraActor.h"

UEngine* GEngine = nullptr;

bool UEngine::Init()
{
	AssetManager = NewObject<UAssetManager>();
	/*World = FObjectFactory::ConstructObject<UWorld>();

	if (!World || !World->Init()) return false;*/

 	return true;
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType WorldType)
{
	FWorldContext* NewWorldContext = new FWorldContext();
	WorldContextlist.Add(NewWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName();

	return *NewWorldContext;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldContextlist)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromType(EWorldType WorldType)
{
	for (FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == WorldType) return &Context;
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromPIEInstance(int32 PIEInstance)
{
	for (FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == PIEInstance) return &Context;
	return nullptr;
}

UWorld* UEngine::GetEditorWorld() const
{
	for (const FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == EWorldType::Editor) return Context.World();
	return nullptr;
}

UWorld* UEngine::CreatePIEWorldByDuplication(FWorldContext& Context, UWorld* SourceWorld)
{
	UWorld* World = UWorld::GetDuplicatedWorldForPIE(SourceWorld);
	if (World) Context.SetCurrentWorld(World);
	return World;
}

void UEngine::DestroyWorldContext(UWorld* InWorld)
{
	for (int32 idx = 0; idx < WorldContextlist.Num(); ++idx)
	{
		if (WorldContextlist[idx].World() == InWorld)
		{
			const EWorldType ContextType = WorldContextlist[idx].WorldType;
			if (WorldContextlist[idx].GameViewport)
			{
				WorldContextlist[idx].GameViewport->LostFocus();
				WorldContextlist[idx].GameViewport->Reset();
			}
			if (WorldContextlist[idx].OwningGameInstance)
			{
				UGameInstance* Instance = WorldContextlist[idx].OwningGameInstance;
				Instance->Shutdown();
				delete Instance;
			}
			WorldContextlist[idx].SetCurrentWorld(nullptr);
			WorldContextlist.RemoveAt(idx);
			if (InWorld && ContextType == EWorldType::PIE)
			{
				ACameraActor* Camera = InWorld->GetMainCamera();
				const bool bOwnsCamera = Camera && !Camera->GetLevel();
				InWorld->SetMainCamera(nullptr);
				InWorld->ClearWorld();
				for (ULevel* Level : InWorld->GetLevels()) delete Level;
				if (bOwnsCamera) delete Camera;
				delete InWorld;
			}
			break;
		}
	}
}

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	ThisCurrentWorld = World;
}
