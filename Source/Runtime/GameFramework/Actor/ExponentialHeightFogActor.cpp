#include "EnginePCH.h"
#include "ExponentialHeightFogActor.h"
#include "Asset/AssetManager.h"

AExponentialHeightFogActor::AExponentialHeightFogActor()
{
	ExponentialHeightFogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>("UExponentialHeightFogComponent");
	SetRootComponent(ExponentialHeightFogComponent);

	// 아이콘용 메시/머티리얼. Plane은 FVertex 포맷이라 DefaultShader로 그대로 그려진다.
	// Fog 아이콘 텍스처가 준비되면 Property 창에서 머티리얼 슬롯에 끼우면 된다.

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetHiddenInGame(true);
	BillboardComponent->SetupAttachment(ExponentialHeightFogComponent);
	BillboardComponent->SetHiddenInDetails(true);

	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
    UTexture2D* Icon = UAssetManager::Get().LoadTexture("Assets/Icons/Fog.png");
    if (Base && Icon)
    {
        UMaterial* IconMat = UMaterial::CreateInstance(Base);
        IconMat->Textures[0] = Icon;                 
        BillboardComponent->SetMaterial(0, IconMat);
    }
}
