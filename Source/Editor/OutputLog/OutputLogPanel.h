#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineLog.h"



struct FLogData
{
	FString message;
	ImVec4 Color;
};

class FOutputLogPanel : public IEditorPanel, public ILogSink
{
public:
	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Output Log"; }

	void ClearLog();

	//template<typename... Args>
	//void AddLog(ELogVerbosity Verbosity, std::format_string<Args...> fmt, Args&&... args)
	//{

	//}

	void ExecCommand(const FString& command_line);

	int TextEditCallback(ImGuiInputTextCallbackData* data);

private:
	char                  InputBuf[256];
	TArray<FLogData>       Items;
	TArray<FString>		  Commands;
	TArray<FString>       History;
	int                   HistoryPos;
	ImGuiTextFilter       Filter;
	bool                  AutoScroll;
	bool                  ScrollToBottom;

	// Inherited via ILogSink
	void OnLog(ELogVerbosity, const FString& Message) override;
};