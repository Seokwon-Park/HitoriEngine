#include "EnginePCH.h"
#include "EditorCommand.h"

#include "Input/InputSystem.h"
#include "Render/Texture2D.h"

namespace
{
	bool IsModifierDown(EKeyCode Generic, EKeyCode Left, EKeyCode Right)
	{
		return FInputSystem::IsKeyDown(Generic) || FInputSystem::IsKeyDown(Left) || FInputSystem::IsKeyDown(Right);
	}
}

bool FInputChord::IsPressedThisFrame() const
{
	if (!IsValid() || !FInputSystem::IsKeyPressed(Key))
		return false;

	// 수식키는 "정확히 일치"해야 Alt+P와 P가 구분된다.
	const bool bCtrlDown = IsModifierDown(EKeyCode::Control, EKeyCode::LControl, EKeyCode::RControl);
	const bool bAltDown = IsModifierDown(EKeyCode::Alt, EKeyCode::LAlt, EKeyCode::RAlt);
	const bool bShiftDown = IsModifierDown(EKeyCode::Shift, EKeyCode::LShift, EKeyCode::RShift);
	return bCtrlDown == bCtrl && bAltDown == bAlt && bShiftDown == bShift;
}

FString FInputChord::ToString() const
{
	if (!IsValid())
		return "";

	FString Result;
	if (bCtrl)  Result += "Ctrl+";
	if (bAlt)   Result += "Alt+";
	if (bShift) Result += "Shift+";

	const int32 Code = static_cast<int32>(Key);
	if ((Code >= 'A' && Code <= 'Z') || (Code >= '0' && Code <= '9'))
		Result += static_cast<char>(Code);
	else if (Key == EKeyCode::Escape) Result += "Esc";
	else if (Key == EKeyCode::Pause)  Result += "Pause";
	else if (Code >= static_cast<int32>(EKeyCode::F1) && Code <= static_cast<int32>(EKeyCode::F12))
		Result += "F" + std::to_string(Code - static_cast<int32>(EKeyCode::F1) + 1);
	return Result;
}

namespace EditorCommandUI
{
	bool ToolbarButton(const char* Id, const FEditorCommand& Command, float IconSize)
	{
		if (!Command.IsVisibleNow())
			return false;

		const bool bEnabled = Command.CanExecuteNow();
		ImGui::BeginDisabled(!bEnabled);

		bool bClicked = false;
		if (Command.Icon)
		{
			// 비활성일 때는 BeginDisabled가 알파를 낮춰 준다.
			bClicked = ImGui::ImageButton(Id, (ImTextureID)Command.Icon->GetResource()->GetSRV(),
				{ IconSize, IconSize }, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, Command.IconTint);
		}
		else
		{
			ImGui::PushID(Id);
			ImGui::PushStyleColor(ImGuiCol_Text, Command.IconTint);
			bClicked = ImGui::Button(Command.Label);
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		ImGui::EndDisabled();

		// 비활성 버튼에도 툴팁이 뜨도록 AllowWhenDisabled
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			const FString Chord = Command.Shortcut.ToString();
			if (Chord.empty()) ImGui::SetTooltip("%s", Command.Tooltip);
			else               ImGui::SetTooltip("%s (%s)", Command.Tooltip, Chord.c_str());
		}

		if (bClicked)
			Command.TryExecute();
		return bClicked;
	}

	bool MenuItem(const FEditorCommand& Command)
	{
		if (!Command.IsVisibleNow())
			return false;

		const FString Chord = Command.Shortcut.ToString();
		if (ImGui::MenuItem(Command.Label, Chord.empty() ? nullptr : Chord.c_str(), false, Command.CanExecuteNow()))
			return Command.TryExecute();
		return false;
	}

	void ProcessShortcuts(const TArray<const FEditorCommand*>& Commands)
	{
		// 텍스트 입력 중에는 단축키를 먹지 않는다 (Details 이름 편집 등)
		if (ImGui::GetIO().WantTextInput)
			return;

		for (const FEditorCommand* Command : Commands)
		{
			// 같은 키(Pause/Resume)를 공유하는 커맨드가 한 프레임에 둘 다 실행되지 않도록 첫 성공에서 멈춘다.
			if (Command && Command->Shortcut.IsPressedThisFrame() && Command->TryExecute())
				return;
		}
	}
}