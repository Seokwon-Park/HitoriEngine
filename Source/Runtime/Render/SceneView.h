#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/Frustum.h"
#include "Math/Rect.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include <algorithm>

// Immutable description of one camera view for the duration of a render.
struct FSceneView
{
	int32 ViewIndex = 0;
	FVector ViewLocation{};
	FRotator ViewRotation{};
	FVector ViewForward{};
	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FMatrix ViewProjectionMatrix = FMatrix::Identity;
	FFrustumPlanes Frustum{};
	FRect ViewRect{};
	float FieldOfView = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	float OrthoWidth = 10.0f;
	bool bIsPerspective = true;
	bool bIsWireframe = false;
	bool bGameView = false;

	FViewContext ToViewContext() const
	{
		FViewContext Context{};
		Context.ViewIndex = ViewIndex;
		Context.Width = static_cast<uint32>(ViewRect.Width);
		Context.Height = static_cast<uint32>(ViewRect.Height);
		Context.CameraPosition = ViewLocation;
		Context.CameraForward = ViewForward;
		Context.ViewProjection = ViewProjectionMatrix;
		Context.NearZ = NearClip;
		Context.bOrthographic = !bIsPerspective;
		Context.bGameView = bGameView;
		const float ScaleX = ProjectionMatrix.M[1][0];
		const float ScaleY = ProjectionMatrix.M[2][1];
		Context.ProjectionScaleSquared = std::max(ScaleX * ScaleX, ScaleY * ScaleY);
		Context.Prepare();
		return Context;
	}

	FViewContext ToLODViewContext() const
	{
		return ToViewContext();
	}
};
