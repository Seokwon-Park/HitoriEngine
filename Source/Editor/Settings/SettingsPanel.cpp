#include "EnginePCH.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Engine/World.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "GameFramework/DefaultPawn.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectIterator.h"

// 종료 시 렌더·에디터·뷰포트 설정을 함께 저장한다.
FSettingsPanel::~FSettingsPanel()
{
	ViewportsPanel = nullptr;
	SaveSettings();
}

// 설정 파일을 읽어 패널 상태를 초기화한다.
bool FSettingsPanel::Init()
{
	LoadSettings();
	return true;
}


// 설정 패널의 프레임 갱신 진입점이다.
void FSettingsPanel::Tick(float DeltaTime)
{
}

// ImGui 조작으로 렌더·카메라 설정과 저장·복원 요청을 처리한다.
void FSettingsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Settings");

	//////////////////////////////////////////////////////////

	// 렌더링 옵션 (Toggles)
	ImGui::SeparatorText("Rendering");

	ImGui::TextDisabled("Fill mode is configured per viewport.");
	ImGui::Checkbox("Draw Primitives", &Settings.bDrawPrimitives);
	ImGui::Checkbox("Draw Bounding Box", &Settings.bDrawBoundingBox);
	ImGui::Checkbox("Show Object UUID", &Settings.bShowUUID);
	const char* RenderingPaths[] = { "Forward", "Deferred" };
	int RenderingPathIndex = static_cast<int>(Settings.RenderingPath);
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::Combo("Rendering Path", &RenderingPathIndex, RenderingPaths, IM_ARRAYSIZE(RenderingPaths)))
		Settings.RenderingPath = static_cast<ERenderingPath>(RenderingPathIndex);

	const char* LightingModels[] = { "Gouraud", "Lambert", "Phong" };
	int LightingModelIndex = static_cast<int>(Settings.LightingModel);
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::Combo("Lighting Model", &LightingModelIndex, LightingModels, IM_ARRAYSIZE(LightingModels)))
		Settings.LightingModel = static_cast<ELightingModel>(LightingModelIndex);
	const char* LightCullingModes[] = { "Off", "Tiled", "Clustered", "Tiled 2.5D" };
	int LightCullingModeIndex = static_cast<int>(Settings.LightCullingMode);
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::Combo("Light Culling", &LightCullingModeIndex,
		LightCullingModes, IM_ARRAYSIZE(LightCullingModes)))
	{
		Settings.LightCullingMode = static_cast<ELightCullingMode>(LightCullingModeIndex);
	}
	ImGui::TextDisabled("Forward Lambert/Phong only; Off evaluates all uploaded local lights.");
	ImGui::TextDisabled("Forward Lambert/Phong only; off evaluates all visible local lights.");

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Scene Depth");
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::DragFloat("Max Distance", &Settings.SceneDepthMaxRange, 0.5f,
		0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp))
		ApplySceneDepthRange();
	ImGui::TextDisabled("White represents this distance from the camera.");

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Anti-Aliasing");

	ImGui::Checkbox("Enable FXAA", &Settings.bEnableFXAA);

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Grid");

	ImGui::Checkbox("Draw Batch Line / Grid", &Settings.bDrawBatchLine);
	ImGui::Checkbox("Draw PS Grid", &Settings.bDrawPSGrid);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderInt("Grid Spacing", &Settings.GridSpacing, 1, 100);

	//////////////////////////////////////////////////////////

	// 에디터 수치 설정 (Values)
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Editor Settings");

	UCameraComponent* CamCom = World->GetMainCamera()->GetCameraComponent();

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Rotate Sensitivity", &Settings.MouseSensitivity, 0.01f, 1.0f, "%.2f");
	CamCom->SetMouseSensitivity(Settings.MouseSensitivity);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Speed", &Settings.CameraSpeed, 0.1f, 10.0f, "%.2f");
	CamCom->SetMoveSpeed(Settings.CameraSpeed);

	//////////////////////////////////////////////////////////
	DrawDefaultPawnSettings();

	//////////////////////////////////////////////////////////
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Load Settings");

	if (ImGui::Button("Load Settings"))
	{
		LoadSettings();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Load settings from editor.ini");
	}

	ImGui::SameLine();

	if (ImGui::Button("Save Settings"))
	{
		SaveSettings();
	}

	//////////////////////////////////////////////////////////

	ImGui::End();
}

// 알려진 설정 섹션을 editor.ini에 함께 기록해 뷰포트 값 유실을 막는다.
bool FSettingsPanel::SaveSettings() const
{
	FEditorSettings Snapshot = Settings;
	ReadViewportSettings(Snapshot);
	std::ofstream File("editor.ini");

	if (!File.is_open())
	{
		HTR_LOG(Error, "Failed to open editor.ini for saving.");
		return false;
	}

	File << "[Rendering]\n";
	File << "Wireframe=" << Settings.bWireframe << "\n";
	File << "DrawPrimitives=" << Settings.bDrawPrimitives << "\n";
	File << "DrawBoundingBox=" << Settings.bDrawBoundingBox << "\n";
	File << "ShowUUID=" << Settings.bShowUUID << "\n";
	File << "DrawBatchLine=" << Settings.bDrawBatchLine << "\n";
	File << "DrawPSGrid=" << Settings.bDrawPSGrid << "\n";
	File << "SceneDepthMaxRange=" << Settings.SceneDepthMaxRange << "\n";
	File << "EnableFXAA=" << Settings.bEnableFXAA << "\n";
	File << "LightCullingMode=" << (Settings.LightCullingMode == ELightCullingMode::Off ? "Off" : Settings.LightCullingMode == ELightCullingMode::Tiled ? "Tiled" :
		Settings.LightCullingMode == ELightCullingMode::Clustered ? "Clustered" : "Tiled25D") << "\n";
	File << "RenderingPath=" << (Settings.RenderingPath == ERenderingPath::Forward ? "Forward" : "Deferred") << "\n";
	File << "LightingModel=" << (Settings.LightingModel == ELightingModel::Gouraud ? "Gouraud" :
		Settings.LightingModel == ELightingModel::Lambert ? "Lambert" : "Phong") << "\n";
	File << "\n";

	File << "[Editor]\n";
	File << "CameraMoveSpeed=" << Settings.CameraSpeed << "\n";
	File << "CameraRotateSensitivity=" << Settings.MouseSensitivity << "\n";
	File << "GridSpacing=" << Settings.GridSpacing << "\n";
	File << "\n";

	File << "[MultipleViewports]\n";
	File << "Horizontal=" << Snapshot.MultipleViewportsHorizontal << "\n";
	File << "Vertical=" << Snapshot.MultipleViewportsVertical << "\n";
	File << "Layout=" << (Snapshot.bMultipleViewportsSingle ? "Single" : "Quad") << "\n";
	File << "SingleViewIndex=" << Snapshot.MultipleViewportsSingleViewIndex << "\n";

	for (int32 Index = 0; Index < 4; ++Index)
    {
        File << "View" << Index << "Fov=" << Snapshot.ViewFov[Index] << "\n";
        File << "View" << Index << "OrthoWidth=" << Snapshot.ViewOrthoWidth[Index] << "\n";
        File << "View" << Index << "Preset=" << Snapshot.ViewPreset[Index] << "\n";
        File << "View" << Index << "Wireframe=" << Snapshot.ViewWireframe[Index] << "\n";
        File << "View" << Index << "Mode=" << Snapshot.ViewMode[Index] << "\n";
        File << "View" << Index << "LocationX=" << Snapshot.ViewLocation[Index].X << "\n";
        File << "View" << Index << "LocationY=" << Snapshot.ViewLocation[Index].Y << "\n";
        File << "View" << Index << "LocationZ=" << Snapshot.ViewLocation[Index].Z << "\n";
        File << "View" << Index << "RotationX=" << Snapshot.ViewRotation[Index].X << "\n";
        File << "View" << Index << "RotationY=" << Snapshot.ViewRotation[Index].Y << "\n";
        File << "View" << Index << "RotationZ=" << Snapshot.ViewRotation[Index].Z << "\n";
        File << "View" << Index << "RotationW=" << Snapshot.ViewRotation[Index].W << "\n";
    }
    File.close();
	return true;
}

// 파일의 키를 파싱해 렌더·카메라·레이아웃 설정을 복원한다.
bool FSettingsPanel::LoadSettings()
{
	std::ifstream File("editor.ini");
	if (!File.is_open())
	{
		return false;
	}

	uint8 LocationComponentMasks[4]{};
	uint8 RotationComponentMasks[4]{};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = false;
		Settings.bViewRotationSaved[Index] = false;
	}

	std::string Line;
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == ';' || Line[0] == '[') continue;

		std::istringstream Iss(Line);
		std::string Key;
		if (std::getline(Iss, Key, '='))
		{
			std::string ValueStr;
			if (std::getline(Iss, ValueStr))
			{
				// 새 View 키는 NaN·잘못된 숫자·범위 밖 값을 무시한다.
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    FString Prefix = "View";
                    Prefix += static_cast<char>('0' + Index);
                    std::istringstream Number(ValueStr);
                    float Value = 0;
                    if (!(Number >> Value) || !std::isfinite(Value)) continue;
                    if (Key == Prefix + "Fov" && Value >= 1 && Value <= 179)
                        Settings.ViewFov[Index] = Value;
                    else if (Key == Prefix + "OrthoWidth" && Value >= 0.01f)
                        // 파일 값은 절대 상한으로 먼저 제한하고, 실제 View 비율 상한은 Adapter가 적용한다.
                        Settings.ViewOrthoWidth[Index] = FMath::Clamp(Value, 0.01f, 1000000.0f);
                    else if (Key == Prefix + "Preset" && Value >= 0 && Value <= 7 && Value == static_cast<int32>(Value))
                        Settings.ViewPreset[Index] = static_cast<int32>(Value);
                    else if (Key == Prefix + "Wireframe" && (Value == 0 || Value == 1))
                        Settings.ViewWireframe[Index] = static_cast<int32>(Value);
					else if (Key == Prefix + "Mode" && Value >= 0 &&
						Value <= static_cast<int32>(EViewportMode::LightCulling) && Value == static_cast<int32>(Value))
						Settings.ViewMode[Index] = static_cast<int32>(Value);
                    else if (Key == Prefix + "LocationX")
                    {
                        Settings.ViewLocation[Index].X = Value;
                        LocationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "LocationY")
                    {
                        Settings.ViewLocation[Index].Y = Value;
                        LocationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "LocationZ")
                    {
                        Settings.ViewLocation[Index].Z = Value;
                        LocationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationX")
                    {
                        Settings.ViewRotation[Index].X = Value;
                        RotationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "RotationY")
                    {
                        Settings.ViewRotation[Index].Y = Value;
                        RotationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "RotationZ")
                    {
                        Settings.ViewRotation[Index].Z = Value;
                        RotationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationW")
                    {
                        Settings.ViewRotation[Index].W = Value;
                        RotationComponentMasks[Index] |= 1 << 3;
                    }
                }
                if (Key == "Wireframe") Settings.bWireframe = std::stoi(ValueStr);
				else if (Key == "DrawPrimitives") Settings.bDrawPrimitives = std::stoi(ValueStr);
				else if (Key == "DrawBoundingBox") Settings.bDrawBoundingBox = std::stoi(ValueStr);
				else if (Key == "ShowUUID") Settings.bShowUUID = std::stoi(ValueStr);
				else if (Key == "DrawBatchLine") Settings.bDrawBatchLine = std::stoi(ValueStr);
				else if (Key == "DrawPSGrid") Settings.bDrawPSGrid = std::stoi(ValueStr);
				else if (Key == "SceneDepthMaxRange") Settings.SceneDepthMaxRange = std::stof(ValueStr);
				else if (Key == "EnableFXAA") Settings.bEnableFXAA = std::stoi(ValueStr);
				else if (Key == "LightCullingMode")
				{
					if (ValueStr == "Off") Settings.LightCullingMode = ELightCullingMode::Off;
					else if (ValueStr == "Tiled") Settings.LightCullingMode = ELightCullingMode::Tiled;
					else if (ValueStr == "Clustered") Settings.LightCullingMode = ELightCullingMode::Clustered;
					else if (ValueStr == "Tiled25D") Settings.LightCullingMode = ELightCullingMode::Tiled25D;
				}
				else if (Key == "EnableTiledLightCulling" || Key == "EnableClusteredLightCulling")
				{
					Settings.LightCullingMode = std::stoi(ValueStr) != 0 ? ELightCullingMode::Tiled : ELightCullingMode::Off;
				}
				else if (Key == "RenderingPath")
				{
					if (ValueStr == "Forward") Settings.RenderingPath = ERenderingPath::Forward;
					else if (ValueStr == "Deferred") Settings.RenderingPath = ERenderingPath::Deferred;
				}
				else if (Key == "LightingModel")
				{
					if (ValueStr == "Gouraud") Settings.LightingModel = ELightingModel::Gouraud;
					else if (ValueStr == "Lambert") Settings.LightingModel = ELightingModel::Lambert;
					else if (ValueStr == "Phong") Settings.LightingModel = ELightingModel::Phong;
				}

				else if (Key == "CameraMoveSpeed") Settings.CameraSpeed = std::stof(ValueStr);
				else if (Key == "CameraRotateSensitivity") Settings.MouseSensitivity = std::stof(ValueStr);
				else if (Key == "GridSpacing") Settings.GridSpacing = std::stoi(ValueStr);
				else if (Key == "Horizontal") Settings.MultipleViewportsHorizontal = std::stof(ValueStr);
				else if (Key == "Vertical") Settings.MultipleViewportsVertical = std::stof(ValueStr);
				else if (Key == "Layout") Settings.bMultipleViewportsSingle = ValueStr == "Single";
				else if (Key == "SingleViewIndex")
				{
					const int32 ViewIndex = std::stoi(ValueStr);
					Settings.MultipleViewportsSingleViewIndex = ViewIndex >= 0 && ViewIndex < 4 ? ViewIndex : 0;
				}
			}
		}
	}

	File.close();
	if (!std::isfinite(Settings.SceneDepthMaxRange))
		Settings.SceneDepthMaxRange = 50.0f;
	Settings.SceneDepthMaxRange = FMath::Clamp(Settings.SceneDepthMaxRange, 0.1f, 10000.0f);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = LocationComponentMasks[Index] == 0x7;
		if (RotationComponentMasks[Index] == 0xF)
		{
			const FQuat& Rotation = Settings.ViewRotation[Index];
			const float LengthSquared = Rotation.X * Rotation.X + Rotation.Y * Rotation.Y +
				Rotation.Z * Rotation.Z + Rotation.W * Rotation.W;
			if (LengthSquared > 1.0e-12f)
			{
				Settings.ViewRotation[Index] = Rotation.Normalized();
				Settings.bViewRotationSaved[Index] = true;
			}
		}
	}
	ApplyViewportSettings();
	return true;
}

// 뷰포트 설정 복사
void FSettingsPanel::ReadViewportSettings(FEditorSettings& Out) const
{
	if (!ViewportsPanel)
		return;

	for (int32 Index = 0; Index < 4; ++Index)
	{
		FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(Index);
		if (!Client)
			continue;

		Out.ViewFov[Index] = Client->GetViewFOV();
		Out.ViewOrthoWidth[Index] = Client->GetOrthoWidth();
		Out.ViewWireframe[Index] = Client->IsWireframe() ? 1 : 0;
		Out.ViewMode[Index] = static_cast<int32>(Client->GetViewportMode());
		Out.ViewLocation[Index] = Client->GetViewLocation();
		Out.ViewRotation[Index] = Client->GetViewRotation().Quaternion();
	}
}

// 종료 시 설정 갱신
void FSettingsPanel::CaptureViewportSettings() { ReadViewportSettings(Settings); }

// 뷰포트 설정 적용
void FSettingsPanel::ApplyViewportSettings()
{
	if (!ViewportsPanel)
		return;
	ApplySceneDepthRange();

	for (int32 Index = 0; Index < 4; ++Index)
	{
		FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(Index);
		if (!Client)
			continue;

		if (Settings.ViewFov[Index] > 0.0f)
			Client->SetViewFOV(Settings.ViewFov[Index]);
		if (Settings.ViewWireframe[Index] >= 0)
			Client->SetViewportMode(Settings.ViewWireframe[Index] == 1 ? EViewportMode::Wireframe : EViewportMode::Solid);
		if (Settings.ViewMode[Index] >= 0)
			Client->SetViewportMode(static_cast<EViewportMode>(Settings.ViewMode[Index]));
	}
}

void FSettingsPanel::ApplySceneDepthRange()
{
	if (!ViewportsPanel)
		return;

	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(Index))
			Client->SetMaxRange(Settings.SceneDepthMaxRange);
	}
}

void FSettingsPanel::DrawDefaultPawnSettings()
{
    ImGui::Dummy(ImVec2(0.0f, SectionGap));
    ImGui::SeparatorText("DefaultPawn Settings");
    ADefaultPawn* Pawn = World ? Cast<ADefaultPawn>(World->GetPlayerPawn()) : nullptr;
    if (!Pawn)
    {
        ImGui::TextDisabled("Start PIE with a DefaultPawn to edit these settings.");
        return;
    }

    const auto DrawMeshPicker = [](const char* Label, UStaticMesh*& SelectedMesh)
    {
        const auto GetMeshDisplayName = [](const UStaticMesh* Mesh) -> FString
        {
            if (!Mesh) return "None";
            const fs::path Path(Mesh->GetPath());
            return Path.stem() == "Atlas"
                ? Path.parent_path().filename().string()
                : Path.stem().string();
        };
        const FString Preview = GetMeshDisplayName(SelectedMesh);
        bool bChanged = false;
        const bool bOpen = ImGui::BeginCombo(Label, Preview.c_str());
        if (SelectedMesh && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", SelectedMesh->GetPath().c_str());
        if (bOpen)
        {
            if (ImGui::Selectable("None", SelectedMesh == nullptr))
            {
                SelectedMesh = nullptr;
                bChanged = true;
            }
            for (TObjectIterator<UStaticMesh> Mesh; Mesh; ++Mesh)
            {
                const FString Name = GetMeshDisplayName(*Mesh);
                const bool bSelected = *Mesh == SelectedMesh;
                ImGui::PushID(*Mesh);
                if (ImGui::Selectable(Name.c_str(), bSelected))
                {
                    SelectedMesh = *Mesh;
                    bChanged = true;
                }
                if (bSelected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Mesh->GetPath().c_str());
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return bChanged;
    };

    UStaticMeshComponent* PawnMesh = Pawn->GetStaticMeshComponent();
    if (PawnMesh)
    {
        UStaticMesh* Mesh = PawnMesh->GetStaticMesh();
        if (DrawMeshPicker("Pawn Mesh", Mesh)) PawnMesh->SetStaticMesh(Mesh);
    }
    UStaticMesh* ProjectileMesh = Pawn->GetProjectileMesh();
    if (DrawMeshPicker("Projectile Mesh", ProjectileMesh)) Pawn->SetProjectileMesh(ProjectileMesh);
    if (!ProjectileMesh) ImGui::TextDisabled("No mesh selected; fires the default sphere.");
    ImGui::TextDisabled("Projectile settings apply to the next shot.");
}
