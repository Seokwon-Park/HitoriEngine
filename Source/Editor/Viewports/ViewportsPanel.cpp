#include "EnginePCH.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Engine/GameViewportClient.h"

#include "Core/StatOverlay.h"
#include "Core/Stats/LightweightStats.h"
#include "Render/RenderCommand.h"

#include <algorithm>
#include <cassert>
#include <format>

#include "Core/Stats/EditorStats.h"

namespace {
constexpr float SplitterThickness = 6.0f;
constexpr ImU32 SplitterColor = IM_COL32(55, 55, 55, 230);
constexpr ImU32 SplitterHoverColor = IM_COL32(255, 192, 0, 255);
constexpr const char *ViewportTypeLabels[] = {
		"Perspective", "Top", "Front", "Right"};

// Stat Overlay
constexpr float StatOverlayMargin = 8.0f;
constexpr float StatOverlayPadding = 6.0f;
constexpr ImU32 StatOverlayBackgroundColor = IM_COL32(0, 0, 0, 140);
constexpr ImU32 TitleColor = IM_COL32(255, 210, 60, 255);
constexpr ImU32 ValueColor = IM_COL32(235, 235, 235, 255);
} // namespace

// 네 View의 렌더 타깃을 최소 크기로 초기화한다.
bool FViewportsPanel::Init() {
	for (int32 i = 0; i < 4; ++i) {
		ViewportClients[i] = MakeUnique<FEditorViewportClient>();
		ViewportClients[i]->SetViewIndex(i);
		ViewportClients[i]->Resize(1, 1);
	}

	// 초기 시점 설정
	ViewportClients[0]->SetViewportType(ELevelViewportType::Perspective);
	ViewportClients[1]->SetViewportType(ELevelViewportType::Top);
	ViewportClients[2]->SetViewportType(ELevelViewportType::Front);
	ViewportClients[3]->SetViewportType(ELevelViewportType::Right);

	return true;
}

// 패널의 프레임 갱신 인터페이스이며 별도 계산은 하지 않는다.
void FViewportsPanel::Tick(float DeltaTime) { (void)DeltaTime; }

// Core Rect에 맞춰 View 활성 상태와 타깃 크기를 갱신한다.
void FViewportsPanel::SetView(const int32 ViewIndex, const FRect &Rect,
			const bool bActive) {
	assert(ViewIndex >= 0 && ViewIndex < 4);
	FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
	if (!Client)
		return;

	Client->SetRect(Rect);
	const bool bSlotActive = bActive && Rect.Width > 0.0f && Rect.Height > 0.0f;
	Client->SetActive(bSlotActive);
	if (!bSlotActive)
		return;

	const uint32 Width = static_cast<uint32>(std::max(1.0f, Rect.Width));
	const uint32 Height = static_cast<uint32>(std::max(1.0f, Rect.Height));
	if (Width != Client->GetWidth() || Height != Client->GetHeight())
		Client->Resize(Width, Height);
}

// 마우스 위치에서 패널 원점을 빼 로컬 좌표로 바꾼다.
FVector2 FViewportsPanel::GetLocalMousePosition() const {
	POINT Cursor{};
	const ImVec2 Mouse = ::GetCursorPos(&Cursor)
		? ImVec2(static_cast<float>(Cursor.x), static_cast<float>(Cursor.y))
		: ImGui::GetMousePos();
	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

// 엔진 입력 처리는 ImGui NewFrame보다 먼저 실행되므로 현재 OS 커서로 버튼
// 영역을 검사한다.
bool FViewportsPanel::IsHovered() const {
	if (bStatResetButtonVisible && FStatOverlay::IsAnyEnabled()) {
		POINT Cursor{};
		if (::GetCursorPos(&Cursor) && Cursor.x >= StatResetButtonMin.x &&
				Cursor.x < StatResetButtonMax.x && Cursor.y >= StatResetButtonMin.y &&
				Cursor.y < StatResetButtonMax.y)
			return false;
	}
	const FVector2 Mouse = GetLocalMousePosition();
	return bHovered && Mouse.X >= 0.0f && Mouse.Y >= 0.0f &&
		Mouse.X < ContentSize.x && Mouse.Y < ContentSize.y;
}

// 누적 가로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeHorizontalDrag() {
	const float Result = PendingHorizontalDrag;
	PendingHorizontalDrag = 0.0f;
	return Result;
}

// 누적 세로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeVerticalDrag() {
	const float Result = PendingVerticalDrag;
	PendingVerticalDrag = 0.0f;
	return Result;
}

// 대기 Layout 요청을 한 번 반환하고 플래그를 지운다.
bool FViewportsPanel::ConsumeLayoutRequest(ELayoutMode &OutMode,
			int32 &OutSingleViewIndex) {
	if (!bHasLayoutRequest)
		return false;
	OutMode = RequestedLayoutMode;
	OutSingleViewIndex = RequestedSingleViewIndex;
	CurrentLayoutMode = RequestedLayoutMode;
	CurrentSingleViewIndex = RequestedSingleViewIndex;
	bHasLayoutRequest = false;
	return true;
}

// View Texture와 Splitter·Layout·Preset UI를 그리고 요청을 기록한다.
void FViewportsPanel::OnRender() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	ImGui::Begin("Viewports", nullptr,
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoTitleBar);

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();
	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);
	bHovered = ImGui::IsWindowHovered();
	bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	bStatResetButtonVisible = false;

	// 전체 캔버스를 한 번 확보한 뒤 각 렌더 타깃을 창 DrawList에 직접 그린다.
	// Image 항목 네 개를 따로 배치하면 ImGui 레이아웃과 클리핑 상태가 삽입 순서에
	// 영향을 받아, Core가 올바른 사각형을 줘도 아래쪽 행이 잘릴 수 있다.
	ImGui::Dummy(ContentSize);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(ContentOrigin,
		{ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		FEditorViewportClient* Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive())
			continue;

		const FRect &Rect = Client->GetRect();
		const ImVec2 ViewMin{ContentOrigin.x + Rect.X,
			ContentOrigin.y + Rect.Y};
		const ImVec2 ViewMax{ViewMin.x + Rect.Width,
			ViewMin.y + Rect.Height};

		const bool bGameViewport = GameViewportClient && ViewIndex == GameViewportIndex;
		if (ViewIndex == 0 && bShowNoCamera && !bGameViewport)
		{
			// 메인 원근 뷰포트 영역을 어두운 회색으로 채움
			DrawList->AddRectFilled(ViewMin, ViewMax, IM_COL32(45, 45, 48, 255));
			const char* WarningText = "No Camera";
			ImVec2 TextSize = ImGui::CalcTextSize(WarningText);
			ImVec2 CenterPos{
				ViewMin.x + (Rect.Width - TextSize.x) * 0.5f,
				ViewMin.y + (Rect.Height - TextSize.y) * 0.5f
			};
			DrawList->AddText(CenterPos, IM_COL32(220, 220, 220, 255), WarningText);
			continue;
		}

		FTexture2D* DisplayTarget = bGameViewport
			? GameViewportClient->GetColorTarget()
			: Client->GetColorTarget();
		if (!DisplayTarget || !DisplayTarget->GetSRV())
			continue;

		DrawList->AddImage(DisplayTarget->GetSRV(), ViewMin, ViewMax);

		// 마우스 클릭 시 활성 뷰포트 설정
		const bool bViewHovered = bHovered && ImGui::IsMouseHoveringRect(ViewMin, ViewMax);
		if (bViewHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
				ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
				ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
			ActiveViewIndex = ViewIndex;
			ImGui::SetWindowFocus();
			bFocused = true;
		}
	}
	DrawList->PopClipRect();

	if (CurrentLayoutMode == ELayoutMode::QuadSplit && ViewportClients[0] && ViewportClients[1] && ViewportClients[2]) {
		const FRect &Rect0 = ViewportClients[0]->GetRect();
		const FRect &Rect1 = ViewportClients[1]->GetRect();
		const FRect &Rect2 = ViewportClients[2]->GetRect();
		// 분할선 배치 계산
		const float SplitX =
				(Rect0.X + Rect0.Width + Rect1.X) * 0.5f;
		const float SplitY =
				(Rect0.Y + Rect0.Height + Rect2.Y) * 0.5f;

		const ImVec2 VerticalMin{
				ContentOrigin.x + SplitX - SplitterThickness * 0.5f, ContentOrigin.y};
		const ImVec2 VerticalMax{VerticalMin.x + SplitterThickness,
			ContentOrigin.y + ContentSize.y};
		const ImVec2 HorizontalMin{ContentOrigin.x, ContentOrigin.y + SplitY -
			SplitterThickness * 0.5f};
		const ImVec2 HorizontalMax{ContentOrigin.x + ContentSize.x,
			HorizontalMin.y + SplitterThickness};

		ImGui::SetCursorScreenPos(VerticalMin);
		ImGui::InvisibleButton("##MultipleViewportsHorizontalSplitter",
													 {SplitterThickness, ContentSize.y});
		ImGui::SetCursorScreenPos(HorizontalMin);
		ImGui::InvisibleButton("##MultipleViewportsVerticalSplitter",
													 {ContentSize.x, SplitterThickness});

		const bool bVerticalHovered =
				bHovered && ImGui::IsMouseHoveringRect(VerticalMin, VerticalMax);
		const bool bHorizontalHovered =
				bHovered && ImGui::IsMouseHoveringRect(HorizontalMin, HorizontalMax);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			bDraggingVerticalSplitter = bVerticalHovered;
			bDraggingHorizontalSplitter = bHorizontalHovered;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			bDraggingVerticalSplitter = false;
			bDraggingHorizontalSplitter = false;
		}

		const bool bVerticalHighlighted =
				bDraggingVerticalSplitter || bVerticalHovered;
		const bool bHorizontalHighlighted =
				bDraggingHorizontalSplitter || bHorizontalHovered;
		DrawList->AddRectFilled(VerticalMin, VerticalMax,
			bVerticalHighlighted ? SplitterHoverColor
			: SplitterColor);
		DrawList->AddRectFilled(HorizontalMin, HorizontalMax,
			bHorizontalHighlighted ? SplitterHoverColor
			: SplitterColor);

		if (bDraggingVerticalSplitter)
			PendingHorizontalDrag += ImGui::GetIO().MouseDelta.x;
		if (bDraggingHorizontalSplitter)
			PendingVerticalDrag += ImGui::GetIO().MouseDelta.y;

		if (bVerticalHighlighted && bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (bVerticalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else if (bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	} else {
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
	}

	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex) {
		FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive())
			continue;
		const FRect &Rect = Client->GetRect();
		ImGui::SetCursorScreenPos(
				{ContentOrigin.x + Rect.X + 8.0f,
				 ContentOrigin.y + Rect.Y + 8.0f});
		ImGui::PushID(100 + ViewIndex);
		int SelectedType = static_cast<int>(Client->GetViewportType());
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##ViewportType", &SelectedType, ViewportTypeLabels,
			IM_ARRAYSIZE(ViewportTypeLabels))) {
			Client->SetViewportType(static_cast<ELevelViewportType>(SelectedType));
		}
		ImGui::SameLine();
		// 장면 채우기 모드 설정
		int Mode = static_cast<int>(Client->GetViewportMode());
		const char *Labels[] = {"Solid", "Wireframe", "Scene Depth", "World Normal", "Light Culling"};
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##FillMode", &Mode, Labels, IM_ARRAYSIZE(Labels))) {
			Client->SetViewportMode(static_cast<EViewportMode>(Mode));
		}
		ImGui::SameLine();
		if (CurrentLayoutMode == ELayoutMode::QuadSplit) {
			if (ImGui::SmallButton("Single")) {
				RequestedLayoutMode = ELayoutMode::Single;
				RequestedSingleViewIndex = ViewIndex;
				bHasLayoutRequest = true;
			}
		} else if (ViewIndex == CurrentSingleViewIndex &&
			ImGui::SmallButton("Quad")) {
			RequestedLayoutMode = ELayoutMode::QuadSplit;
			RequestedSingleViewIndex = ViewIndex;
			bHasLayoutRequest = true;
		}
		if (ViewIndex == ActiveViewIndex && !Client->IsLightCulling() &&
				FStatOverlay::IsAnyEnabled() &&
				(FStatOverlay::IsEnabled(EStatFlags::Profile) ||
				 FStatRegistry::Find(EditorStats::STAT_PickingTime))) {
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset Stats"))
				FStatRegistry::Reset();
			StatResetButtonMin = ImGui::GetItemRectMin();
			StatResetButtonMax = ImGui::GetItemRectMax();
			bStatResetButtonVisible = true;
		}
		ImGui::PopID();
	}



	// 마지막으로 선택된 뷰포트만 오버레이
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex) {
		if (ViewIndex != ActiveViewIndex)
			continue;
		FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive() || Client->IsLightCulling())
			continue;
		DrawStatOverlay(DrawList, {ContentOrigin.x + Client->GetRect().X,
			ContentOrigin.y + Client->GetRect().Y});
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// 항목별 줄을 모아 한 번에 배경과 텍스트를 그린다.
void FViewportsPanel::DrawStatOverlay(ImDrawList *DrawList,
			const ImVec2 &ViewMin) const {
	if (!DrawList || !FStatOverlay::IsAnyEnabled())
		return;

	constexpr float BytesPerMegabyte = 1024.0f * 1024.0f;

	// 제목은 UE처럼 노란색, 값은 흰색으로 구분한다.
	struct FStatLine {
		FString Text;
		ImU32 Color;
	};
	TArray<FStatLine> Lines;

	if (FStatOverlay::IsEnabled(EStatFlags::FPS)) {
		Lines.Add({"FPS", TitleColor});
		Lines.Add(
				{std::format("  {:.1f} fps", FStatOverlay::GetFPS()), ValueColor});
		Lines.Add({std::format("  {:.2f} ms", FStatOverlay::GetFrameTimeMs()),
			ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Memory)) {
		Lines.Add({"Memory", TitleColor});
		Lines.Add({std::format("  Object  {:.2f} MB ({} allocs)",
			static_cast<double>(
			FStatOverlay::GetObjectAllocationBytes()) /
			BytesPerMegabyte,
			FStatOverlay::GetObjectAllocationCount()),
			ValueColor});
		Lines.Add({std::format("  Process {:.2f} MB",
			static_cast<double>(
			FStatOverlay::GetProcessWorkingSetBytes()) /
			BytesPerMegabyte),
			ValueColor});
	}

	struct FProfileStatLine {
		FString Text;
		ImU32 Color;
		FString Values[3];
	};
	TArray<FProfileStatLine> ProfileLines;
	if (FStatOverlay::IsEnabled(EStatFlags::Profile)) {
		ProfileLines.Add({"CPU Profile (ms)", TitleColor, {"Last", "Avg", "Max"}});

		for (const auto &[Name, Data] : FStatRegistry::GetAll()) {
			if (TStatId{Name} == EditorStats::STAT_PickingTime)
				continue;

			FProfileStatLine Line{"  " + FString(Name), ValueColor};
			Line.Values[0] = std::format("{:.2f}", Data.GetLastMs());
			Line.Values[1] = std::format("{:.2f}", Data.GetRecentAverageMs());
			Line.Values[2] = std::format("{:.2f}", Data.GetMaxMs());
			ProfileLines.Add(Line);
		}
	}

	if (const FCycleStatData *PickingData =
					FStatRegistry::Find(EditorStats::STAT_PickingTime)) {
		ProfileLines.Add({"Picking", TitleColor});
		FProfileStatLine Line{"  Time (ms)", ValueColor};
		Line.Values[0] = std::format("Last: {:.2f}", PickingData->GetLastMs());
		Line.Values[1] = std::format("Attempts: {}", PickingData->CallCount);
		Line.Values[2] = std::format("Acc.: {:.2f}", PickingData->GetTotalMs());
		ProfileLines.Add(Line);
	}

	if (Lines.Num() == 0 && ProfileLines.Num() == 0)
		return;

	// 제어 위젯 한 줄 아래에서 시작해 Combo와 겹치지 않게 한다.
	const float LineHeight = ImGui::GetTextLineHeight();
	const ImVec2 Origin{ViewMin.x + StatOverlayMargin,
			ViewMin.y + StatOverlayMargin + ImGui::GetFrameHeight() +
			StatOverlayMargin};

	// 최대 너비 계산
	float MaxWidth = 0.0f;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
		MaxWidth =
				std::max(MaxWidth, ImGui::CalcTextSize(Lines[Index].Text.c_str()).x);

	const float MinValueWidth = ImGui::CalcTextSize("00000.00").x;
	float NameWidth = 0.0f;
	float ValueWidths[3] = {MinValueWidth, MinValueWidth, MinValueWidth};
	bool bHasColumns = false;

	for (const FProfileStatLine &Line : ProfileLines) {
		const float TextWidth = ImGui::CalcTextSize(Line.Text.c_str()).x;
		bHasColumns = true;
		NameWidth = std::max(NameWidth, TextWidth);
		for (int32 Column = 0; Column < 3; ++Column)
			ValueWidths[Column] = std::max(
					MinValueWidth, ImGui::CalcTextSize(Line.Values[Column].c_str()).x);
	}

	float ColumnRight[3]{};
	if (bHasColumns) {
		float Width = NameWidth;
		for (int32 Column = 0; Column < 3; ++Column) {
			Width += ImGui::GetFontSize() + ValueWidths[Column];
			ColumnRight[Column] = Origin.x + Width;
		}
		MaxWidth = std::max(MaxWidth, Width);
	}

	const ImVec2 BackgroundMin{Origin.x - StatOverlayPadding,
			Origin.y - StatOverlayPadding};
	const ImVec2 BackgroundMax{
			Origin.x + MaxWidth + StatOverlayPadding,
			Origin.y +
					LineHeight * static_cast<float>(Lines.Num() + ProfileLines.Num()) +
					StatOverlayPadding};
	DrawList->AddRectFilled(BackgroundMin, BackgroundMax,
			StatOverlayBackgroundColor, 4.0f);

	// Lines 그리기
	for (int32 Index = 0; Index < Lines.Num(); ++Index) {
		DrawList->AddText(
				{Origin.x, Origin.y + LineHeight * static_cast<float>(Index)},
				Lines[Index].Color, Lines[Index].Text.c_str());
	}

	// ProfileLines 그리기
	for (int32 Index = 0; Index < ProfileLines.Num(); ++Index) {
		const FProfileStatLine &Line = ProfileLines[Index];
		const float Y =
				Origin.y + LineHeight * static_cast<float>(Lines.Num() + Index);
		DrawList->AddText({Origin.x, Y}, Line.Color, Line.Text.c_str());

		for (int32 Column = 0; Column < 3; ++Column) {
			const FString &Text = Line.Values[Column];
			const float TextWidth = ImGui::CalcTextSize(Text.c_str()).x;
			DrawList->AddText({ColumnRight[Column] - TextWidth, Y}, Line.Color,
			Text.c_str());
		}
	}
}
