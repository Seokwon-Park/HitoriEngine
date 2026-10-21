#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"

#include "Engine/World.h"

struct FTransform;

using DetailsSelectionCallback = std::function<void(UActorComponent*)>;
using DetailsDeleteComponentCallback = std::function<void(UActorComponent*)>;

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

	void SelectComponent(UActorComponent* Component);
	void DrawCompoenetList(AActor* SelectedActor, UActorComponent*& OutSelectedComponent);

	const char* GetPanelName() const override { return "Details"; }

	void SetTarget(AActor* InTargetActor) 
	{ 
		TargetActor = InTargetActor;
		TargetComponent = InTargetActor ? InTargetActor->GetRootComponent() : nullptr;
	}
	void SetTargetComponent(UActorComponent* InTargetComponent) { TargetComponent = InTargetComponent; }

	void SetSelectionCallback(DetailsSelectionCallback InCallback) { Callback = InCallback; }
	void SetDeleteComponentCallback(DetailsDeleteComponentCallback InCallback) { DeleteCallback = InCallback; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	ImFont* GetCustomFont() { return CustomFont; }

private:
	void DrawSceneComponentNode(USceneComponent* Component, USceneComponent* Root, UActorComponent*& OutSelectedComponent);
	void TryReparent(USceneComponent* DroppedComponent, USceneComponent* DragComponent);

	UWorld* World = nullptr;
	ImFont* CustomFont = nullptr;

	AActor* TargetActor = nullptr;
	UActorComponent* TargetComponent = nullptr;
	DetailsSelectionCallback Callback;
	DetailsDeleteComponentCallback DeleteCallback;

	UActorComponent* PendingDeleteComponent = nullptr;
	UActorComponent* EditingNameComponent = nullptr;
	char ComponentNameEditBuffer[256] = {};
	bool bFocusNameEdit = false;
	bool bIsEditingName = false;

	USceneComponent* PendingDroppedComponent = nullptr;
	USceneComponent* PendingDragComponent = nullptr;
};

