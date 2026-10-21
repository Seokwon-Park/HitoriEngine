#pragma once

#include "Core/Types.h"

// PIE를 어디에 띄울지 고른다. UE의 Play 버튼 옆 모드 선택에 해당한다.
enum class EPlayModeType : uint8
{
	InViewport,         // 선택한 패널 View에서 실행
	InEditorFloating,   // 별도 OS 창에서 실행
};

enum class EPIEState : uint8
{
	Stopped,
	Playing,
	Paused,
	Simulating
};

struct FRequestPlaySessionParams
{
public:
	// Default Constructor
	FRequestPlaySessionParams()
		//: SessionDestination(EPlaySessionDestinationType::InProcess)
		//, WorldType(EPlaySessionWorldType::PlayInEditor)
		//, EditorPlaySettings(nullptr)
		//, bAllowOnlineSubsystem(true)
	{
	}

	//struct FLauncherDeviceInfo
	//{
	//	/** The id of the Device selected in the Launch drop-down to launch on. Platform name is to the left of any @ symbol. */
	//	FString DeviceId;
	//	/** The name of the Device selected in the Launch drop-down to launch on. */
	//	FString DeviceName;
	//	/** If True, a remote play session will attempt to update the flash/software on the target device if it's out of date */
	//	bool bUpdateDeviceFlash = false;
	//	/** If True, the launch device is a Simulator */
	//	bool bIsSimulator = false;
	//	/** If set, the architecture used for building and deploying to the device */
	//	FString Architecture;
	//};

	///** Where should the session be launched? May be local or remote. */
	//EPlaySessionDestinationType SessionDestination;

	////Play In Editor vs Play In Simulation
	//EPlaySessionWorldType WorldType;

	///** If set, preview an emulated version of some rendering specifications. SessionDestination should be set to match. */
	//TOptional<EPlaySessionPreviewType> SessionPreviewTypeOverride;

	///** A ULevelEditorPlaySettings instance that the session should be started with. nullptr means use the CDO. */
	//TObjectPtr<ULevelEditorPlaySettings> EditorPlaySettings;

	///** If this is set the Play session will start from this location instead of using the GameMode to find a Player Spawn. */
	//TOptional<FVector> StartLocation;

	///** If StartLocation is set, this can be used to optionally override the default (FRotator::ZeroRotator) rotation. */
	//TOptional<FRotator> StartRotation;

	///**
	//* If specified, the Play Session will be created inside of this Slate Viewport.
	//* Only supported on EPlaySessionDestinationType::InProcess destinations.
	//*/
	//TOptional<TWeakPtr<class IAssetViewport>> DestinationSlateViewport;

	///**
	//* If specified, the play session will attempt to launch on a remote device and
	//* not in the local editor. Requires EPlaySessionDestinationType::Launcher.
	//*/
	//TOptional<FLauncherDeviceInfo> LauncherTargetDevice;

	///**
	//* If specified, this will be appended to the parameters being passed to the stand
	//* executable. Requires EPlaySessionDestinationType::NewProcess.
	//*/
	//TOptional<FString> AdditionalStandaloneCommandLineParameters;

	///** If specified, the PIE instance will be created inside this user-provided window instead of creating a default. */
	//TWeakPtr<SWindow> CustomPIEWindow;

	//TSubclassOf<AGameModeBase> GameModeOverride;

	///** Override which map is loaded for the Play session. This overrides both offline & servers (server only can be overridden in ULevelEditorPlaySettings) */
	//FString GlobalMapOverride;

	///** If false, then PIE won't try to ask the Online Subsystem/"Use Online Logins" feature if it should authenticate the PIE sessions. */
	//bool bAllowOnlineSubsystem;

	//bool HasPlayWorldPlacement() const
	//{
	//	return WorldType != EPlaySessionWorldType::SimulateInEditor && StartLocation.IsSet();
	//}
};
