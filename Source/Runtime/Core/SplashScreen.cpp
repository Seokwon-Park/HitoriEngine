#include "EnginePCH.h"
#include "SplashScreen.h"

#include "Render/ImageLoader.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace
{
	constexpr float SplashScreenRatio = 0.4f;
	constexpr const wchar_t* SplashClassName = L"HitoriSplash";
	constexpr int32 ProgressBarHeight = 6;
	constexpr int32 TextMargin = 16;

	struct FSplashState
	{
		std::thread Thread;
		std::mutex Mutex;
		std::condition_variable WindowReady;
		bool bWindowCreated = false;

		HINSTANCE hInstance = nullptr;
		HWND hWnd = nullptr;
		HBITMAP Bitmap = nullptr;
		int32 Width = 0;
		int32 Height = 0;
		int32 ImageWidth = 0;   
		int32 ImageHeight = 0;

		std::wstring Text;
		float Progress = 0.0f;
	};

	FSplashState GSplash;

	HBITMAP CreateBitmapFromImage(const FImageData& Image)
	{
		BITMAPINFO Info{};
		Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		Info.bmiHeader.biWidth = static_cast<LONG>(Image.Width);
		Info.bmiHeader.biHeight = -static_cast<LONG>(Image.Height);   // 음수 = 위에서 아래로 (stb 순서)
		Info.bmiHeader.biPlanes = 1;
		Info.bmiHeader.biBitCount = 32;
		Info.bmiHeader.biCompression = BI_RGB;

		void* Bits = nullptr;
		HBITMAP Bitmap = CreateDIBSection(nullptr, &Info, DIB_RGB_COLORS, &Bits, nullptr, 0);
		if (!Bitmap)
		{
			return nullptr;
		}

		const uint8* Src = Image.Pixels.GetData();
		uint8* Dst = static_cast<uint8*>(Bits);
		const uint32 PixelCount = Image.Width * Image.Height;
		for (uint32 i = 0; i < PixelCount;++i)
		{
			Dst[i * 4 + 0] = Src[i * 4 + 2];
			Dst[i * 4 + 1] = Src[i * 4 + 1];
			Dst[i * 4 + 2] = Src[i * 4 + 0];
			Dst[i * 4 + 3] = 255;
		}

		return Bitmap;
	}

	void Paint(HWND hWnd)
	{
		PAINTSTRUCT Ps;
		HDC ScreenDC = BeginPaint(hWnd, &Ps);

		RECT Client;
		GetClientRect(hWnd, &Client);
		const int32 W = Client.right;
		const int32 H = Client.bottom;

		HDC MemDC = CreateCompatibleDC(ScreenDC);
		HBITMAP BackBuffer = CreateCompatibleBitmap(ScreenDC, W, H);
		HGDIOBJ OldBackBuffer = SelectObject(MemDC, BackBuffer);

		if (GSplash.Bitmap)
		{
			HDC ImageDC = CreateCompatibleDC(ScreenDC);
			HGDIOBJ OldImage = SelectObject(ImageDC, GSplash.Bitmap);

			SetStretchBltMode(MemDC, HALFTONE);
			SetBrushOrgEx(MemDC, 0, 0, nullptr);
			StretchBlt(MemDC, 0, 0, W, H,
				ImageDC, 0, 0, GSplash.ImageWidth, GSplash.ImageHeight, SRCCOPY);

			SelectObject(ImageDC, OldImage);
			DeleteDC(ImageDC);
		}
		else
		{
			FillRect(MemDC, &Client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
		}

		std::wstring Text;
		float Progress;
		{
			std::lock_guard Lock(GSplash.Mutex);
			Text = GSplash.Text;
			Progress = GSplash.Progress;
		}

		RECT BarBack = { 0 , H - ProgressBarHeight, W, H };
		HBRUSH BackBrush = CreateSolidBrush(RGB(40, 40, 40));
		FillRect(MemDC, &BarBack, BackBrush);
		DeleteObject(BackBrush);

		RECT BarFill = { 0, H - ProgressBarHeight, static_cast<LONG>(W * Progress), H };
		HBRUSH FillBrush = CreateSolidBrush(RGB(0, 150, 255));
		FillRect(MemDC, &BarFill, FillBrush);
		DeleteObject(FillBrush);

		SetBkMode(MemDC, TRANSPARENT);
		SetTextColor(MemDC, RGB(255, 255, 255));
		HGDIOBJ OldFont = SelectObject(MemDC, GetStockObject(DEFAULT_GUI_FONT));
		RECT TextRect = { TextMargin, 0, W - TextMargin, H - ProgressBarHeight - TextMargin / 2 };
		DrawTextW(MemDC, Text.c_str(), -1, &TextRect, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
		SelectObject(MemDC, OldFont);

		BitBlt(ScreenDC, 0, 0, W, H, MemDC, 0, 0, SRCCOPY);

		SelectObject(MemDC, OldBackBuffer);
		DeleteObject(BackBuffer);
		DeleteDC(MemDC);
		EndPaint(hWnd, &Ps);
	}

	LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch (Msg)
		{
		case WM_PAINT:
			Paint(hWnd);
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_CLOSE:
			DestroyWindow(hWnd);
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(hWnd, Msg, wParam, lParam);
	}

	void SplashThreadMain()
	{
		WNDCLASSEXW Wc{};
		Wc.cbSize = sizeof(WNDCLASSEXW);
		Wc.lpfnWndProc = SplashWndProc;
		Wc.hInstance = GSplash.hInstance;
		Wc.hCursor = LoadCursorW(nullptr, IDC_APPSTARTING);
		Wc.lpszClassName = SplashClassName;

		RegisterClassExW(&Wc);

		const int32 X = (GetSystemMetrics(SM_CXSCREEN) - GSplash.Width) / 2;
		const int32 Y = (GetSystemMetrics(SM_CYSCREEN) - GSplash.Height) / 2;

		HWND hWnd = CreateWindowExW(
			WS_EX_TOOLWINDOW | WS_EX_TOPMOST,   // 작업 표시줄에 안 뜨고, 항상 위
			SplashClassName, L"", WS_POPUP,     // 테두리·제목 표시줄 없음
			X, Y, GSplash.Width, GSplash.Height,
			nullptr, nullptr, GSplash.hInstance, nullptr);

		{
			std::lock_guard Lock(GSplash.Mutex);
			GSplash.hWnd = hWnd;
			GSplash.bWindowCreated = true;
		}
		GSplash.WindowReady.notify_one();

		if (!hWnd) return;

		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);

		MSG Msg;
		while (GetMessageW(&Msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&Msg);
			DispatchMessageW(&Msg);
		}

		UnregisterClassW(SplashClassName, GSplash.hInstance);

	}
}




void FSplashScreen::Show(HINSTANCE hInstance, const FString& ImagePath)
{
	if (GSplash.Thread.joinable())
	{
		return;
	}

	const FImageData Image = ImageLoader::Load(ImagePath);
	if (Image.IsValid())
	{
		GSplash.Bitmap = CreateBitmapFromImage(Image);
		GSplash.ImageWidth = static_cast<int32>(Image.Width);
		GSplash.ImageHeight = static_cast<int32>(Image.Height);

		// 화면의 Ratio 크기 상자 안에 원본 비율을 유지한 채 맞춘다.
		const float MaxW = GetSystemMetrics(SM_CXSCREEN) * SplashScreenRatio;
		const float MaxH = GetSystemMetrics(SM_CYSCREEN) * SplashScreenRatio;
		const float Scale = std::min(MaxW / Image.Width, MaxH / Image.Height);
		GSplash.Width = static_cast<int32>(Image.Width * Scale);
		GSplash.Height = static_cast<int32>(Image.Height * Scale);
	}
	else
	{
		GSplash.Width = 600;
		GSplash.Height = 340;
	}

	GSplash.hInstance = hInstance;
	GSplash.bWindowCreated = false;
	GSplash.Thread = std::thread(SplashThreadMain);

	std::unique_lock Lock(GSplash.Mutex);
	GSplash.WindowReady.wait(Lock, [] {return GSplash.bWindowCreated;});
}

void FSplashScreen::SetText(const FString& Text)
{
	std::wstring WString(Text.begin(), Text.end());
	SetText(WString);
}

void FSplashScreen::SetText(const std::wstring& Text)
{
	HWND hWnd;
	{
		std::lock_guard Lock(GSplash.Mutex);
		GSplash.Text = Text;
		hWnd = GSplash.hWnd;
	}

	if (hWnd)
	{
		InvalidateRect(hWnd, nullptr, FALSE);
	}
}

void FSplashScreen::SetProgress(float Ratio)
{
	HWND hWnd;
	{
		std::lock_guard Lock(GSplash.Mutex);
		GSplash.Progress = FMath::Clamp(Ratio, 0.0f, 1.0f);
		hWnd = GSplash.hWnd;
	}
	if (hWnd)
	{
		InvalidateRect(hWnd, nullptr, FALSE);
	}
}

void FSplashScreen::Hide()
{
	if (!GSplash.Thread.joinable())
	{
		return;
	}

	HWND hWnd;
	{
		std::lock_guard Lock(GSplash.Mutex);
		hWnd = GSplash.hWnd;
		GSplash.hWnd = nullptr;   // 이후 SetText가 닫힌 창을 건드리지 않게
	}
	if (hWnd)
	{
		PostMessageW(hWnd, WM_CLOSE, 0, 0);   // 창은 만든 스레드만 파괴할 수 있으므로 메시지로 요청한다
	}
	GSplash.Thread.join();

	if (GSplash.Bitmap)
	{
		DeleteObject(GSplash.Bitmap);
		GSplash.Bitmap = nullptr;
	}
	GSplash.Text.clear();
	GSplash.Progress = 0.0f;
}
