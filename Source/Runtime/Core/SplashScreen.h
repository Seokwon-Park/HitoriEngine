#pragma once

#include <Windows.h>

class FSplashScreen
{
public:
	static void Show(HINSTANCE hInstance, const FString& ImagePath);
	static void SetText(const FString& Text);
	static void SetText(const std::wstring& Text);

	static void SetProgress(float Ratio);
	static void Hide();

};