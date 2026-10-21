#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <filesystem>

namespace fs = std::filesystem;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);

private:
	fs::path AssetRootPath = "Assets";
	fs::path CurrentPath = "Assets";

	const FString FolderIconPath = "Assets/Icons/Folder.png";
	const FString FileIconPath = "Assets/Icons/File.png";
	
};