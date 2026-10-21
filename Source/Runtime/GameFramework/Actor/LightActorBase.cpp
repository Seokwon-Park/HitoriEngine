#include "EnginePCH.h"

#include "LightActorBase.h"
#include "Asset/AssetManager.h"

ALightActorBase::ALightActorBase() {}

void ALightActorBase::SetupLightRoot(ULightComponent* LightComponent, const FString& IconPath)
{
    SetRootComponent(LightComponent);

    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
    BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetHiddenInGame(true);
    BillboardComponent->SetIconSize(2.0f);
    BillboardComponent->SetupAttachment(LightComponent);
    BillboardComponent->SetTintSource(LightComponent);

    UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
    UTexture2D* Icon = UAssetManager::Get().LoadTexture(IconPath);
    if (Base && Icon)
    {
        UMaterial* IconMat = UMaterial::CreateInstance(Base);
        IconMat->Textures[0] = Icon;                 
        BillboardComponent->SetMaterial(0, IconMat);
    }
}
