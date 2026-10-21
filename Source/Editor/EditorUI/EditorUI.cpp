#include "EnginePCH.h"
#include "Editor/EditorUI/EditorUI.h"


bool FEditorUI::Init(bool bInUseDockSpace, bool bInPassthruCentralNode)
{
	bUseDockSpace = bInUseDockSpace;
	bPassthruCentralNode = bInPassthruCentralNode;

	Toolbar = MakeUnique<FLevelEditorToolbar>();

	return true;
}

void FEditorUI::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{

		Panel->Tick(DeltaTime);
	}
}

void FEditorUI::OnRender()
{
	DrawMainMenuBar();

	if (Toolbar) Toolbar->Draw();

	if (bUseDockSpace)
	{
		static bool dockspaceOpen = true;
		static bool optFullscreen = true;
		static bool optPadding = false;
		static ImGuiDockNodeFlags dockspaceFlags = bPassthruCentralNode
			? ImGuiDockNodeFlags_PassthruCentralNode
			: ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		//ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
		if (optFullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!optPadding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Daydream::Application::GetInstance().isMaximzed ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, false ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		ImGui::Begin("DockingSpace", &dockspaceOpen, windowFlags);
		//m_DockSpacePos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
		//STS_CORE_WARN("Dock Space Coord = {0}, {1}", m_DockSpacePos.x, m_DockSpacePos.y);
		ImGui::PopStyleColor(); // MenuBarBg
		ImGui::PopStyleVar(2);

		if (!optPadding)
			ImGui::PopStyleVar();

		if (optFullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("EngineDockingSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		ImGui::End();
	}

	for (auto& Panel : Panels)
	{
		if (Panel->IsOpen())
		{
			Panel->OnRender();
		}
	}
}

// 파일 메뉴의 요청을 애플리케이션 콜백으로 전달한다.
void FEditorUI::DrawMainMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { if (OnNewScene) OnNewScene(); }
			if (ImGui::MenuItem("Open Scene...")) { if (OnOpenScene) OnOpenScene(); }

			ImGui::Separator();

			if (ImGui::MenuItem("Save")) { if (OnSaveScene) OnSaveScene(); }
			if (ImGui::MenuItem("Save As...")) { if (OnSaveSceneAs) OnSaveSceneAs(); }

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			for (auto& Panel : Panels)
			{
				bool bOpen = Panel->IsOpen();

				if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, bOpen))
				{
					Panel->SetOpen(!bOpen);
				}
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar(2);
}
