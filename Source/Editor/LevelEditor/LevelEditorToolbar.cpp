#include "EnginePCH.h"
#include "LevelEditorToolbar.h"

#include "Asset/AssetManager.h"

namespace
{
	void BeginToolbarGroup()
	{
		ImDrawList* DL = ImGui::GetWindowDrawList();
		DL->ChannelsSplit(2);
		DL->ChannelsSetCurrent(1);
		ImGui::BeginGroup();
	}

	void EndToolbarGroup()
	{
		ImGui::EndGroup();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		DL->ChannelsSetCurrent(0);
		DL->AddRectFilled({ Min.x - 4, Min.y - 2 }, { Max.x + 4, Max.y + 2 }, IM_COL32(45, 45, 45, 255), 4.0f);
		DL->ChannelsMerge();
	}

	void ToolbarSeparator()
	{
		ImGui::SameLine(0, 12);
		const ImVec2 P = ImGui::GetCursorScreenPos();
		const float H = ImGui::GetFrameHeight();
		ImGui::GetWindowDrawList()->AddLine({ P.x, P.y }, { P.x, P.y + H }, IM_COL32(70, 70, 70, 255));
		ImGui::Dummy({ 1, H });
		ImGui::SameLine(0, 12);
	}
}

void FLevelEditorToolbar::Draw()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 6 });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4, 4 });
	const bool bOpen = ImGui::BeginViewportSideBar("##LevelEditorToolbar", ImGui::GetMainViewport(),
		ImGuiDir_Up, Height, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

	if (bOpen)
	{
		DrawFileGroup();
		ToolbarSeparator();
		DrawPlayGroup();
	}
	ImGui::End();                       // Begin 결과와 관계없이 항상 End
	ImGui::PopStyleVar(2);
}

void FLevelEditorToolbar::DrawFileGroup()
{
	//EditorCommandUI::ToolbarButton("##Save", Save);
}

void FLevelEditorToolbar::DrawPlayGroup()
{
	BeginToolbarGroup();

	// 같은 자리에 Play/Pause/Resume을 모두 넘긴다. IsVisible이 참인 하나만 그려진다.
	// SameLine은 "그려졌을 때만" 붙여야 빈 간격이 생기지 않는다.
	bool bFirst = true;
	auto Button = [&](const char* Id, const FEditorCommand& Command)
		{
			if (!Command.IsVisibleNow())
				return;
			if (!bFirst) ImGui::SameLine();
			EditorCommandUI::ToolbarButton(Id, Command);
			bFirst = false;
		};

	for (auto Command : Commands)
	{
		Button(Command.Label, Command);
	}
	if (!bFirst) ImGui::SameLine();
	DrawPlayModeMenu();
	//Button("##Play", Play);
	//Button("##Pause", Pause);
	//Button("##Resume", Resume);
	//Button("##Step", StepFrame);
	//Button("##Stop", Stop);

	EndToolbarGroup();
}

void FLevelEditorToolbar::DrawPlayModeMenu()
{
	if (!GetPlayMode || !SetPlayMode)
		return;

	// 실행 중에는 모드를 바꿔도 현재 세션에 반영되지 않으므로 막는다.
	const bool bCanChange = !CanChangePlayMode || CanChangePlayMode();
	ImGui::BeginDisabled(!bCanChange);
	if (ImGui::ArrowButton("##PlayModeMenu", ImGuiDir_Down))
		ImGui::OpenPopup("PlayModePopup");
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Play Mode: %s",
			GetPlayMode() == EPlayModeType::InViewport ? "Selected Viewport" : "New Editor Window (PIE)");

	if (ImGui::BeginPopup("PlayModePopup"))
	{
		const EPlayModeType Current = GetPlayMode();
		if (ImGui::MenuItem("Selected Viewport", nullptr, Current == EPlayModeType::InViewport))
			SetPlayMode(EPlayModeType::InViewport);
		if (ImGui::MenuItem("New Editor Window (PIE)", nullptr, Current == EPlayModeType::InEditorFloating))
			SetPlayMode(EPlayModeType::InEditorFloating);
		ImGui::EndPopup();
	}
}
