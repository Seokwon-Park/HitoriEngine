#pragma once

#include <imgui.h>
#include "Input/KeyCode.h"

class UTexture2D;

struct FInputChord
{
	EKeyCode Key = static_cast<EKeyCode>(0);
	bool bCtrl = false;
	bool bAlt = false;
	bool bShift = false;

	bool IsValid() const { return Key != static_cast<EKeyCode>(0); }
	bool IsPressedThisFrame() const;          // 이번 프레임에 눌렸고 수식키도 일치하는가
	FString ToString() const;                 // 툴팁용 "Alt+P"
};

struct FEditorCommand
{
	const char* Label;
	const char* Tooltip;
	UTexture2D* Icon = nullptr;
    ImVec4 IconTint = { 1, 1, 1, 1 };      // Play 초록색 등
	FInputChord Shortcut;

    // 바인딩: 무엇을 하나
    std::function<void()> Execute;
    std::function<bool()> CanExecute = [] { return true; };
    std::function<bool()> IsVisible = [] { return true; };

    bool CanExecuteNow() const { return !CanExecute || CanExecute(); }
	bool IsVisibleNow()  const { return !IsVisible || IsVisible(); }

	bool TryExecute() const
	{
		if (!Execute || !IsVisibleNow() || !CanExecuteNow())
			return false;
		Execute();
		return true;
	}
};

namespace EditorCommandUI
{
	// 툴바용 버튼. 보이지 않으면 아무것도 그리지 않고 false.
	bool ToolbarButton(const char* Id, const FEditorCommand& Command, float IconSize = 20.0f);
	// 메뉴용 항목 (MainMenuBar 등에서 재사용)
	bool MenuItem(const FEditorCommand& Command);
	// 등록된 커맨드들의 단축키를 검사해 실행
	void ProcessShortcuts(const TArray<const FEditorCommand*>& Commands);
}