#pragma once

#include <Windows.h>
#include <functional>


// UI 레이어가 창 메시지를 먼저 가로챌 수 있게 하는 훅.
// 런타임이 ImGui를 직접 알지 않도록 방향을 뒤집는다.
// true를 반환하면 그 메시지는 소비된 것으로 보고 더 처리하지 않는다.
using FWndProcHook = bool (*)(HWND, UINT, WPARAM, LPARAM);

class FWindow
{
public:
	// 훅은 등록한 쪽이 수명을 책임진다. 해제는 nullptr을 넘긴다.
	static void SetWndProcHook(FWndProcHook Hook);

	// bBorderless면 테두리 없이 (0,0)에 띄워 클라이언트 영역이 곧 요청 크기가 된다.
	// 창은 숨겨진 채로 만들어진다. 초기화가 끝나면 Show()를 호출한다.
	bool Create(HINSTANCE hInstance, int Width, int Height, const wchar_t* Title, bool bBorderless = false);
	void Show();
	void ProcessMessage(bool& bIsRunning);
	// 메인 창이 아닌 창(PIE 창 등)을 직접 닫는다. 메인 창은 OS 종료 흐름을 따른다.
	void Destroy();

	// false면 닫기 버튼이 앱을 끝내지 않고 ConsumeCloseRequest로 요청만 남긴다.
	void SetQuitOnClose(bool bValue) { bQuitOnClose = bValue; }
	bool ConsumeCloseRequest()
	{
		const bool bResult = bCloseRequested;
		bCloseRequested = false;
		return bResult;
	}
	// false면 등록된 WndProc 훅(ImGui)을 거치지 않는다. ImGui 컨텍스트가 없는 창용이다.
	void SetReceivesWndProcHook(bool bValue) { bReceivesWndProcHook = bValue; }
	bool ReceivesWndProcHook() const { return bReceivesWndProcHook; }

	HWND GetHandle() const { return hWnd;  }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CheckResized()
	{
		if (bIsResized)
		{
			bIsResized = false;
			return true;
		}
		return false;
	}

private:
	bool bIsResized = false;
	bool bIsInSizeMove = false;
	bool bQuitOnClose = true;
	bool bCloseRequested = false;
	bool bReceivesWndProcHook = true;

	HWND hWnd = nullptr;

	uint32 Width = 1280;
	uint32 Height = 720;
 };

