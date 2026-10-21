#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <functional>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;
class USceneComponent;

using SelectionCallback = std::function<void(USceneComponent*)>;
using DeleteActorCallback = std::function<void(AActor*)>;

class FOutlinerPanel : public IEditorPanel
{
public:
    FOutlinerPanel() = default;
    ~FOutlinerPanel() = default;

    void SetSelectionCallback(SelectionCallback InCallback) { Callback = InCallback; }
    void SetDeleteActorCallback(DeleteActorCallback InCallback) { DeleteCallback = InCallback;}

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Outliner"; }

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    void SelectActor(AActor* Actor);

    AActor* GetSelectedActor() const;


private:
    UWorld* World = nullptr;
    UObject* SelectedObject = nullptr;

    SelectionCallback Callback;
    DeleteActorCallback DeleteCallback;
    AActor* PendingDeleteActor = nullptr;
};