#pragma once

#include "Render/StaticMeshData.h"
#include "Core/Types.h"
#include <array>

struct FLODGenerateRequest
{
	//Screen Size 임계값
	std::array<float, 3> ScreenThresholds{ 0.30f, 0.10f, 0.03f };
	bool bSaveToAsset = true;
};

// 생성 결과와 실패 원인을 호출자에게 돌려준다.
struct FLODGenerateResult
{
	bool bSuccess = false;
	uint32 TargetTriangles[4]{};
	uint32 ActualTriangles[4]{};

	// LOD0 대비 부피 비율. 1보다 많이 작으면 단순화하면서 메시가 쪼그라든 것이다.
	float VolumeRatio[4]{ 1.0f, 1.0f, 1.0f, 1.0f };

	uint32 RejectedFeatureEdges = 0;
	uint32 RejectedTopology = 0;
	uint32 RejectedFlips = 0;
	uint32 RejectedError = 0;   // LOD별 허용 오차를 넘어 건너뛴 collapse 수

	FString FailureReason;
};


class FStaticMeshLODGenerator
{
public:
	// 입력: 원본 LOD0
	// 출력: 성공하면 OutLODs[0]=LOD1, [1]=LOD2, [2]=LOD3
	static FLODGenerateResult Generate(const FStaticMeshData& LOD0, TArray<FStaticMeshData>& OutLODs);
};