#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"

#include <functional>

struct FObjIndex
{
	int32 PositionIndex = -1;
	int32 UVIndex = -1;
	int32 NormalIndex = -1;
	int32 FaceIndex = -1;

	bool operator==(const FObjIndex&) const = default;
};

// Cook에서 (P, T, N) 조합으로 정점 중복을 제거할 때 쓰는 해시
struct FObjIndexHash
{
	size_t operator()(const FObjIndex& Index) const
	{
		size_t Hash = std::hash<int32>{}(Index.PositionIndex);
		Hash = Hash * 31 + std::hash<int32>{}(Index.UVIndex);
		Hash = Hash * 31 + std::hash<int32>{}(Index.NormalIndex);
		Hash = Hash * 31 + std::hash<int32>{}(Index.FaceIndex);
		return Hash;
	}
};

struct FObjFace
{
	TArray<FObjIndex> Indexes;
};

struct FObjSection
{
	FString Material;
	int32 MaterialIndex = 0;
	int32 Start = 0; // Faces Index
	int32 Count = 0;
};

struct FObjMaterialInfo
{
	FString Name;
	FVector Kd = FVector(1.0f, 1.0f, 1.0f);
	FString MapKd; // 저장소 루트 기준
	FString MapBump; // 저장소 루트 기준
	float D = 1.0f;
};

struct FObjInfo
{
	FString Path; // 저장소 루트 기준
	TArray<FVector> Positions;
	TArray<FVector2> UVs;
	TArray<FVector> Normals;
	TArray<FVector> Tangents; 
	TArray<FObjFace> Faces;
	TArray<FObjSection> Sections;
	TArray<FString> Mtllibs; // .obj 폴더 기준
	TArray<FObjMaterialInfo> MaterialInfos;
};

