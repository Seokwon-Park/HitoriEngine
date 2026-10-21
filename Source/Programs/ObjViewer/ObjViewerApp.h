//#pragma once
//
//#include "Render/RenderPacket.h"
//
//#include "Engine/Engine.h"
//
//class UObjViewerEngine : public UEngine
//{
//	DECLARE_CLASS(UObjViewerEngine, UEngine)
//
//public:
//	FEngineConfig GetConfig() const override;
//
//	bool Init() override;
//	void Tick(float DeltaTime) override;
//	void PreExit() override;
//
//private:
//	// Frame
//	void HandleShortcuts();
//	void UpdateCamera();
//	void RenderFrame();
//
//	// Mesh
//	bool LoadMesh(const FString& Path);
//	void UpdateWindowTitle();
//
//	// Render
//	void BuildRenderQueue(FRenderQueue& OuTArray) const;
//
//	// Camera
//	void FitCameraToMesh();
//	FVector GetCameraEye() const;
//
//	UStaticMesh* Mesh = nullptr;
//	FString CurrentMeshPath;
//
//	// 오빗 카메라: Target을 중심으로 Yaw·Pitch(도) 방향, Distance만큼 떨어진 위치
//	FVector CameraTarget = FVector(0.0f, 0.0f, 0.0f);
//	float CameraYaw = 0.0f;
//	float CameraPitch = 0.0f;
//	float CameraDistance = 5.0f;
//};
