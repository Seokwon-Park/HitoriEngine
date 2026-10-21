#include "EnginePCH.h"
#include "DirectionalLightActor.h"
#include "Asset/AssetManager.h"

ADirectionalLightActor::ADirectionalLightActor()
{
    DirectionalLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("UDirectionalLightComponent");
	SetupLightRoot(DirectionalLightComponent, "Assets/Icons/Directional.png");
    DirectionalLightComponent->SetRelativeRotation(FRotator(45.0f, 0.0f, 0.0f));

    // Icon 옆에 붙어서 빛의 방향을 알려주는 기즈모 arrow (로컬 x축)
    ArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>("UArrowComponent");
    ArrowComponent->SetEditorIcon(true);
    ArrowComponent->SetupAttachment(DirectionalLightComponent);
    ArrowComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));
    ArrowComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("EditorArrowMaterial"));
    ArrowComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    ArrowComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.5f));
    ArrowComponent->SetHiddenInDetails(true);
    ArrowComponent->SetCastShadow(false);

}