#include "EnginePCH.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"

bool FOutlinerPanel::Init()
{
    return true;
}

void FOutlinerPanel::Tick(float DeltaTime)
{
}

void FOutlinerPanel::OnRender()
{
    // World 참조 확인
    if (!World)
        return;

    // 현재 Level 가져오기
    ULevel* Level = World->GetCurrentLevel();
    if (!Level)
        return;

    // Level의 Actor 목록 Tree 출력
    ImGui::Begin("Outliner");

    DrawActors(Level);

    ImGui::End();

    // Actors 순회가 완전히 끝난 뒤 삭제
    if (PendingDeleteActor)
    {
        if (DeleteCallback)
        {
            DeleteCallback(PendingDeleteActor);
        }

        PendingDeleteActor = nullptr;
    }
}

// Level이 가지고 있는 Actor 목록 순회하며 Draw TreeNode
void FOutlinerPanel::DrawActors(ULevel* Level)
{
    if (!Level)
        return;

    const TArray<AActor*>& Actors = Level->GetActors();

    struct FActorGroup
    {
        EPrimitiveType Type;
        const char* Name;
    };

    const FActorGroup Groups[] =
    {
        { EPrimitiveType::Sphere, "Sphere" },
        { EPrimitiveType::Cube,   "Cube" },
        { EPrimitiveType::Cone,   "Cone" },
        { EPrimitiveType::Plane,  "Plane" }
    };

    ImGuiListClipper Clipper;
	Clipper.Begin(Actors.Num(), ImGui::GetTextLineHeightWithSpacing());

    while (Clipper.Step())
    {
		for (int32 Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
		{
			AActor* Actor = Actors[Index];
			if (Actor)
				DrawActorNode(Actor);
		}
    }

    if (PendingDeleteActor)
    {
        DeleteActorCallback(PendingDeleteActor);
        PendingDeleteActor = nullptr;
    }
}

// Actor를 ImGui TreeNode로 출력
void FOutlinerPanel::DrawActorNode(AActor* Actor)
{
    if (!Actor)
        return;

    const FString& ActorName = Actor->GetName();

    //ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
    ImGuiTreeNodeFlags Flags = 
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    // Actor 선택 시 UI 선택 상태
    if (SelectedObject == Actor)
        Flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx(Actor, Flags, "%s", ActorName.c_str());

    // UI에서 Actor 클릭
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        SelectActor(Actor);
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
        {
            PendingDeleteActor = Actor; // 여기서는 바로 삭제 X
        }

        ImGui::EndPopup();
    }
    
}

// 선택 Object 변경
void FOutlinerPanel::SelectActor(AActor* Actor)
{
    if (!Actor)
    {
        SelectedObject = nullptr;

        if (Callback)
            Callback(nullptr);

        return;
    }

    SelectedObject = Actor;

    HTR_LOG(Info, "{} UUID {} is selected", SelectedObject->GetName(), SelectedObject->GetUUID());

    USceneComponent* Scene = Cast<USceneComponent>(Actor->GetRootComponent());

    if (Callback)
        Callback(Scene);
}


AActor* FOutlinerPanel::GetSelectedActor() const
{
    return Cast<AActor>(SelectedObject);
}