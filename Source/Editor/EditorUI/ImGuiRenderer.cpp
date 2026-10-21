#include "EnginePCH.h"
#include "Editor/EditorUI/ImGuiRenderer.h"

#include "Core/Window.h"
#include "Input/InputSystem.h"

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
	// FWindow가 ImGui를 직접 알지 않도록 메시지 처리기를 훅으로 등록한다.
	bool ImGuiWndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) != 0;
	}

    void ApplyDefaultStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Accent
        const ImVec4 Accent = ImVec4(0.65f, 0.30f, 0.45f, 1.00f);
        const ImVec4 AccentHover = ImVec4(0.75f, 0.40f, 0.55f, 1.00f);
        const ImVec4 AccentActive = ImVec4(0.55f, 0.20f, 0.35f, 1.00f);

        // Text
        colors[ImGuiCol_Text] =
            ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] =
            ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_TextLink] = AccentHover;
        colors[ImGuiCol_TextSelectedBg] =
            ImVec4(1.00f, 1.00f, 1.00f, 0.156f);

        // Window Background
        colors[ImGuiCol_WindowBg] =
            ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
        colors[ImGuiCol_ChildBg] =
            ImVec4(0.280f, 0.280f, 0.280f, 0.00f);
        colors[ImGuiCol_PopupBg] =
            ImVec4(0.313f, 0.313f, 0.313f, 1.00f);
        colors[ImGuiCol_DockingEmptyBg] =
            colors[ImGuiCol_WindowBg];

        // Borders
        colors[ImGuiCol_Border] =
            ImVec4(0.266f, 0.266f, 0.266f, 1.00f);
        colors[ImGuiCol_BorderShadow] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Input Frames
        colors[ImGuiCol_FrameBg] =
            ImVec4(0.160f, 0.160f, 0.160f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] =
            ImVec4(0.200f, 0.200f, 0.200f, 1.00f);
        colors[ImGuiCol_FrameBgActive] =
            ImVec4(0.280f, 0.280f, 0.280f, 1.00f);
        colors[ImGuiCol_InputTextCursor] =
            ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

        // Title & Menu
        colors[ImGuiCol_TitleBg] =
            ImVec4(0.148f, 0.148f, 0.148f, 1.00f);
        colors[ImGuiCol_TitleBgActive] =
            ImVec4(0.148f, 0.148f, 0.148f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] =
            ImVec4(0.148f, 0.148f, 0.148f, 1.00f);
        colors[ImGuiCol_MenuBarBg] =
            ImVec4(0.195f, 0.195f, 0.195f, 1.00f);

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] =
            ImVec4(0.160f, 0.160f, 0.160f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] =
            ImVec4(0.277f, 0.277f, 0.277f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] =
            ImVec4(0.360f, 0.360f, 0.360f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = AccentActive;

        // Checkbox
        // 선택된 박스는 분홍, 체크 표시는 흰색
        colors[ImGuiCol_CheckboxSelectedBg] =
            ImVec4(Accent.x, Accent.y, Accent.z, 0.80f);
        colors[ImGuiCol_CheckMark] =
            ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

        // Slider
        colors[ImGuiCol_SliderGrab] =
            ImVec4(0.391f, 0.391f, 0.391f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = AccentHover;

        // Button
        colors[ImGuiCol_Button] =
            ImVec4(Accent.x, Accent.y, Accent.z, 0.80f);
        colors[ImGuiCol_ButtonHovered] = AccentHover;
        colors[ImGuiCol_ButtonActive] = AccentActive;

        // Header, TreeNode, Selectable
        colors[ImGuiCol_Header] =
            ImVec4(0.313f, 0.313f, 0.313f, 1.00f);
        colors[ImGuiCol_HeaderHovered] =
            ImVec4(0.400f, 0.400f, 0.400f, 1.00f);
        colors[ImGuiCol_HeaderActive] =
            ImVec4(0.469f, 0.469f, 0.469f, 1.00f);

        // Separator
        colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered] =
            ImVec4(0.391f, 0.391f, 0.391f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = Accent;

        // Resize Grip
        colors[ImGuiCol_ResizeGrip] =
            ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] =
            ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = Accent;

        // Tabs
        colors[ImGuiCol_Tab] =
            ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
        colors[ImGuiCol_TabHovered] =
            ImVec4(0.352f, 0.352f, 0.352f, 1.00f);
        colors[ImGuiCol_TabSelected] =
            ImVec4(0.250f, 0.250f, 0.250f, 1.00f);
        colors[ImGuiCol_TabDimmed] =
            ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] =
            ImVec4(0.195f, 0.195f, 0.195f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = Accent;
        colors[ImGuiCol_TabDimmedSelectedOverline] =
            ImVec4(0.40f, 0.30f, 0.35f, 1.00f);

        // Docking
        colors[ImGuiCol_DockingPreview] =
            ImVec4(Accent.x, Accent.y, Accent.z, 0.65f);

        // Graphs
        colors[ImGuiCol_PlotLines] =
            ImVec4(0.469f, 0.469f, 0.469f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = AccentHover;
        colors[ImGuiCol_PlotHistogram] =
            ImVec4(0.586f, 0.586f, 0.586f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = AccentHover;

        // Tables
        colors[ImGuiCol_TableHeaderBg] =
            ImVec4(0.240f, 0.240f, 0.240f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] =
            colors[ImGuiCol_Border];
        colors[ImGuiCol_TableBorderLight] =
            ImVec4(0.220f, 0.220f, 0.220f, 1.00f);
        colors[ImGuiCol_TableRowBg] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] =
            ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

        // Misc
        colors[ImGuiCol_TreeLines] =
            colors[ImGuiCol_Border];
        colors[ImGuiCol_DragDropTarget] = AccentHover;
        colors[ImGuiCol_DragDropTargetBg] =
            ImVec4(Accent.x, Accent.y, Accent.z, 0.18f);
        colors[ImGuiCol_UnsavedMarker] = Accent;
        colors[ImGuiCol_NavCursor] = Accent;
        colors[ImGuiCol_NavWindowingHighlight] = Accent;
        colors[ImGuiCol_NavWindowingDimBg] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.586f);
        colors[ImGuiCol_ModalWindowDimBg] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.586f);

        // ------------------------------------------------
        // Layout & Spacing
        // ------------------------------------------------

        // Padding
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 18.0f;

        // Widget Sizes
        style.GrabMinSize = 10.0f;
        style.ScrollbarSize = 14.0f;

        // Rounding
        style.WindowRounding = 4.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 7.0f;
        style.TabRounding = 3.0f;

        // Borders
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;

        // Minimum Window Size
        style.WindowMinSize = ImVec2(160.0f, 100.0f);
    }

}

FImGuiRenderer::~FImGuiRenderer()
{

}

bool FImGuiRenderer::Init(HWND WindowHandle, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui_ImplWin32_Init(WindowHandle);
	ImGui_ImplDX11_Init(Device, DeviceContext);
	FWindow::SetWndProcHook(&ImGuiWndProcHook);

	ApplyDefaultStyle();

	//// 프로세스가 Per-Monitor DPI Aware라 창이 실제 픽셀 크기로 잡힌다. 모니터 배율(예: 150%)만큼 UI를 키운다.
	//// 1.92+ 동적 폰트 아틀라스라 FontScaleDpi로 키워도 글자가 해당 크기로 다시 래스터화되어 흐려지지 않는다.
	//const float DpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(WindowHandle);
	//ImGuiStyle& Style = ImGui::GetStyle();
	//Style.ScaleAllSizes(DpiScale);     // 패딩·간격·스크롤바 등 크기 (ApplyDefaultStyle 이후에 적용)
	//Style.FontScaleDpi = DpiScale;     // 글자 크기
	//// 다른 배율의 모니터로 창을 옮기면 글자 배율을 자동으로 갱신한다.
	//io.ConfigDpiScaleFonts = true;
	//io.ConfigDpiScaleViewports = true;

	return true;
}

void FImGuiRenderer::Begin()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
}

void FImGuiRenderer::End()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}

void FImGuiRenderer::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
}
