#include "EnginePCH.h"

#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Engine/World.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

#include "Input/InputSystem.h"

#include <cmath>

// 패널 초기화 성공을 반환한다.
bool FEditorControlsPanel::Init()
{
	return true;
}

// Space 입력으로 기즈모 편집 모드를 순환한다.
void FEditorControlsPanel::Tick(float DeltaTime)
{
	if (ImGui::GetIO().WantTextInput)
		return;

	if (Gizmo && FInputSystem::IsKeyPressed(EKeyCode::Space))
	{
		const int NextMode = (static_cast<int>(Gizmo->GetMode()) + 1) % 3;
		Gizmo->SetMode(static_cast<EGizmoMode>(NextMode));
	}
}

// 선택한 클래스의 액터를 월드에 생성한다.
void FEditorControlsPanel::AddActor(uint32 Index)
{
	if (Index >= static_cast<uint32>(Classes.Num())) return;
	World->SpawnActor(Classes[Index]);
}

// 액터 생성·카메라 속성·기즈모·경로 추적 UI를 그린다.
void FEditorControlsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Editor Controls");
	ImGui::Spacing();

	char FpsText[64];
	std::snprintf(FpsText, sizeof(FpsText), "FPS %.1f   %.1f ms", 1.0f / DeltaTime, DeltaTime * 1000.0f);
	float TextWidth = ImGui::CalcTextSize(FpsText).x;
	float CursorX = ImGui::GetCursorPosX();
	float AvailableWidth = ImGui::GetContentRegionAvail().x;

	ImGui::SetCursorPosX(CursorX + AvailableWidth - TextWidth);
	ImGui::TextDisabled("%s", FpsText);

	//////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Actor Spawn");

	const float SpawnButtonWidth = 70.0f;

	char CountText[32];
	std::snprintf(
		CountText,
		sizeof(CountText),
		"%d Actors",
		World->GetActorNum()
	);

	const float CountWidth = ImGui::CalcTextSize(CountText).x;
	const float Available = ImGui::GetContentRegionAvail().x;
	const float Spacing = ImGui::GetStyle().ItemSpacing.x;

	const float ComboWidth =
		Available - SpawnButtonWidth - CountWidth - Spacing * 2.0f;

	ImGui::SetNextItemWidth(ComboWidth);
	ImGui::Combo("##ActorType", &SelectedIndex, Items, IM_ARRAYSIZE(Items));

	ImGui::SameLine();

	if (ImGui::Button("Spawn", ImVec2(SpawnButtonWidth, 0)))
	{
		AddActor(SelectedIndex);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", CountText);

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::SeparatorText("Light Spawn");
	if (ImGui::Button("Directional Light"))
		World->SpawnActor(ADirectionalLightActor::StaticClass());
	ImGui::SameLine();
	if (ImGui::Button("Spot Light"))
		World->SpawnActor(ASpotLightActor::StaticClass());
	ImGui::SameLine();
	if (ImGui::Button("Point Light"))
		World->SpawnActor(APointLightActor::StaticClass());
	ImGui::SameLine();
	if (ImGui::Button("Rect Light"))
		World->SpawnActor(ARectLightActor::StaticClass());
	ImGui::SameLine();
	if (ImGui::Button("Sky Light"))
		World->SpawnActor(ASkyLightActor::StaticClass());

	//////////////////////////////////////////////////////

	DrawCameraProperties();

	//////////////////////////////////////////////////////

	// Gizmo가 없는 앱(Benchmark 등)에서는 이 섹션을 통째로 건너뛴다.
	if (Gizmo)
	{
		ImGui::Dummy(ImVec2(0.0f, SectionGap));
		ImGui::SeparatorText("Gizmo");
	}

	if (Gizmo && ImGui::BeginTable("GizmoControls", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Mode");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		GizmoSelectedIndex = static_cast<int32>(Gizmo->GetMode());
		if (ImGui::Combo("##GizmoMode", &GizmoSelectedIndex, GizmoItems, IM_ARRAYSIZE(GizmoItems)))
		{
			Gizmo->SetMode(static_cast<EGizmoMode>(GizmoSelectedIndex));
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Space");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		if (ImGui::Combo("##GizmoSpace", &SpaceSelectedIndex, SpaceItems, IM_ARRAYSIZE(SpaceItems)))
		{
			Gizmo->SetSpace(static_cast<EGizmoSpace>(SpaceSelectedIndex));
		}

		ImGui::EndTable();
	}

	//////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Path Tracker");

	ImGui::TextDisabled("Recording");

	if (ImGui::BeginTable("RecordingControls", 3))
	{
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button("Start", ImVec2(-1, 0)))
			World->GetPathTracker().SetPathRenderingEnabled(true);

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("Stop", ImVec2(-1, 0)))
			World->GetPathTracker().SetPathRenderingEnabled(false);

		ImGui::TableSetColumnIndex(2);
		if (ImGui::Button("Clear", ImVec2(-1, 0)))
			World->GetPathTracker().ClearPath();

		ImGui::EndTable();
	}

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));

	ImGui::TextDisabled("Replay");

	if (ImGui::BeginTable("ReplayControls", 2))
	{
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button("Play", ImVec2(-1, 0)))
			World->GetPathTracker().SetPlaybackEnabled(true);

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("Stop", ImVec2(-1, 0)))
			World->GetPathTracker().SetPlaybackEnabled(false);

		ImGui::EndTable();
	}

	//////////////////////////////////////////////////////

	ImGui::End();
}

// 실제 View 속성을 편집하며 패널 이동 후에도 마지막 View를 유지하고 변경된 값만 반영한다.
void FEditorControlsPanel::DrawCameraProperties()
{
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Viewport");

	UCameraComponent* CamCom = World && World->GetMainCamera() ? World->GetMainCamera()->GetCameraComponent() : nullptr;
	if (!CamCom) return;

	FEditorViewportClient* ActiveClient = ViewportsPanel ? ViewportsPanel->GetViewportClient(ViewportsPanel->GetActiveViewIndex()) : nullptr;

	bool bOrthogonal = CamCom->GetIsOrthogonal();

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Projection");

	// 카메라 투영 설정 표
	if (ImGui::BeginTable("Camera Projection", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// FOV 또는 Ortho Width
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(bOrthogonal ? "Ortho Width" : "FOV");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		if (bOrthogonal)
		{
			float OrthoWidth = CamCom->GetOrthoWidth();
			if (ImGui::DragFloat("##OrthoWidth", &OrthoWidth, 0.1f) && std::isfinite(OrthoWidth))
			{
				OrthoWidth = FMath::Clamp(OrthoWidth, 0.01f, 1000000.0f);
				CamCom->SetOrthoWidth(OrthoWidth);
				if (ActiveClient) ActiveClient->SetOrthoWidth(OrthoWidth);
			}
		}
		else
		{
			float FOV = CamCom->GetFieldOfView();
			if (ImGui::DragFloat("##FOV", &FOV, 0.1f) && std::isfinite(FOV))
			{
				FOV = FMath::Clamp(FOV, 1.0f, 179.0f);
				CamCom->SetFieldOfView(FOV);
				if (ActiveClient) ActiveClient->SetViewFOV(FOV);
			}
		}

		// 가까운 평면과 먼 평면
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Near");

		ImGui::TableSetColumnIndex(1);
		float Near = CamCom->GetNearZ();

		ImGui::SetNextItemWidth(-1.0f);
		bool Changed = ImGui::InputFloat("##Near", &Near);

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Far");

		ImGui::TableSetColumnIndex(1);
		float Far = CamCom->GetFarZ();
		ImGui::SetNextItemWidth(-1.0f);
		Changed |= ImGui::InputFloat("##Far", &Far);

		if (Changed && std::isfinite(Near) && std::isfinite(Far) && Near > 0 && Far > Near)
		{
			CamCom->SetNearZ(Near);
			CamCom->SetFarZ(Far);
		}
		ImGui::EndTable();
	}

	// 카메라 변환 설정 표
	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Transform");

	bool bLocationChanged = false;
	bool bRotationChanged = false;

	if (ImGui::BeginTable("Camera Transform", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();

		// 카메라 위치
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Location");

		ImGui::TableSetColumnIndex(1);
		float Available = ImGui::GetContentRegionAvail().x;
		float ItemWidth = (Available - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

		FVector Location = CamCom->GetRelativeLocation();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationX", &Location.X, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationY", &Location.Y, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationZ", &Location.Z, 0.1f);

		if (bLocationChanged && std::isfinite(Location.X) && std::isfinite(Location.Y) && std::isfinite(Location.Z))
		{
			CamCom->SetRelativeLocation(Location);
			if (ActiveClient) ActiveClient->SetViewLocation(Location);
		}

		ImGui::TableNextRow();

		// 카메라 회전
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Rotation");

		ImGui::TableSetColumnIndex(1);
		FRotator Rotation = CamCom->GetRelativeRotation();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Pitch", &Rotation.Pitch, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Yaw", &Rotation.Yaw, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		ImGui::BeginDisabled(!bOrthogonal);
		bRotationChanged |= ImGui::DragFloat("##Roll", &Rotation.Roll, 0.1f);
		ImGui::EndDisabled();

		if (bRotationChanged && std::isfinite(Rotation.Pitch) && std::isfinite(Rotation.Yaw) && std::isfinite(Rotation.Roll))
		{
			CamCom->SetRelativeRotation(Rotation);
			if (ActiveClient) ActiveClient->SetViewRotation(Rotation);
		}
		ImGui::EndTable();
	}
}
