#pragma once

#include "Math/EngineMath.h"
#include "Math/Rect.h"
#include "Math/Vector2.h"

#include <cassert>

enum class ELayoutMode { Single, QuadSplit };
enum class EDragAxis { Horizontal, Vertical };

struct FSplitRatio
{
	float Horizontal = 0.5f;
	float Vertical = 0.5f;
};

inline FSplitRatio ClampSplitRatio(const FSplitRatio& Ratio, float MinRatio)
{
	assert(MinRatio >= 0.0f && MinRatio <= 0.5f);
	return {
		FMath::Clamp(Ratio.Horizontal, MinRatio, 1.0f - MinRatio),
		FMath::Clamp(Ratio.Vertical, MinRatio, 1.0f - MinRatio)
	};
}

inline FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, EDragAxis Axis,
	float DeltaPixels, FVector2 WindowSize, float MinRatio)
{
	FSplitRatio Result = Current;
	if (Axis == EDragAxis::Horizontal)
	{
		assert(WindowSize.X > 0.0f);
		Result.Horizontal += DeltaPixels / WindowSize.X;
	}
	else
	{
		assert(WindowSize.Y > 0.0f);
		Result.Vertical += DeltaPixels / WindowSize.Y;
	}
	return ClampSplitRatio(Result, MinRatio);
}

inline void ComputeViewRects(const FSplitRatio& Ratio, FVector2 WindowSize, FRect OutRects[4])
{
	const float SplitX = WindowSize.X * Ratio.Horizontal;
	const float SplitY = WindowSize.Y * Ratio.Vertical;
	OutRects[0] = { 0.0f, 0.0f, SplitX, SplitY };
	OutRects[1] = { SplitX, 0.0f, WindowSize.X - SplitX, SplitY };
	OutRects[2] = { 0.0f, SplitY, SplitX, WindowSize.Y - SplitY };
	OutRects[3] = { SplitX, SplitY, WindowSize.X - SplitX, WindowSize.Y - SplitY };
}

class FLevelViewportLayout
{
public:

private:
};
