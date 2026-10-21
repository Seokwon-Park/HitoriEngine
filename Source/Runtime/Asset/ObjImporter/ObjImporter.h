#pragma once

#include "ObjInfo.h"
#include "Containers/Array.h"
#include "Core/Types.h"
#include "Core/EngineString.h"

struct FStaticMeshData;

class FObjImporter
{
public:
	static TUniquePtr<FStaticMeshData> LoadStaticMeshData(const FString& Path);

	// 디버그용 출력
	static void PrintObjInfo(const FObjInfo& ObjInfo);
	static void PrintSMD(const FStaticMeshData& SMD);

private:
	// .obj 파일 경로 -> FObjInfo 변환
	static bool ParseObj(const FString& Path, FObjInfo& Out);
	// FObjInfo -> FStaticMeshData 변환
	static bool Cook(const FObjInfo& Raw, FStaticMeshData& Out);
};
