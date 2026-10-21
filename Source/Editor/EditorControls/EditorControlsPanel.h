#pragma once
#include "Editor/EditorUI/EditorPanel.h"

#include "Editor/Gizmo/Gizmo.h"

#include "GameFramework/Actor/ParticleActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "GameFramework/Actor/SpotLightActor.h"
#include "GameFramework/Actor/RectLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"
#include "GameFramework/Actor/TextRenderActor.h"
#include "GameFramework/Actor/ExponentialHeightFogActor.h"
#include "GameFramework/Actor/ProjectileActor.h"
class FViewportsPanel;

// 액터 생성과 카메라·기즈모 편집에 필요한 패널 상태를 보관한다.
class FEditorControlsPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Editor Controls"; }
	inline void SetGizmo(FGizmo* InGizmo) { Gizmo = InGizmo; }
	inline void SetWorld(UWorld* InWorld) { World = InWorld; }
	inline void SetViewportsPanel(FViewportsPanel* InPanel) { ViewportsPanel = InPanel; }
	inline void SetIsPlayingQuery(std::function<bool()> InQuery) { IsPlayingQuery = InQuery; }

	float DeltaTime = 1.0f;
	UWorld* World = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;

	void AddActor(uint32 Index);

	int32 SelectedIndex = 0;

	const char* Items[5] = {"StaticMesh", "Particle", "Text", "HeightFog", "Projectile" };

	FGizmo* Gizmo = nullptr;
	int32 GizmoSelectedIndex = 0;
	const char* GizmoItems[3] ={"Location","Rotation","Scale"};

	int32 SpaceSelectedIndex = 0;
	const char* SpaceItems[2] ={"Local","World"};

	TArray<UClass*> Classes
	{
		AStaticMeshActor::StaticClass(),
		AParticleActor::StaticClass(),
		ATextRenderActor::StaticClass(),
		AExponentialHeightFogActor::StaticClass(),
		AProjectileActor::StaticClass(),
	};

private:
	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

    void DrawCameraProperties();
	std::function<bool()> IsPlayingQuery;
};

