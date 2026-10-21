#pragma once

#include "Render/Vertex.h"
#include "StaticMeshData.h"

class FGeometryGenerator
{
public:
	static TUniquePtr<FStaticMeshData> CreateTextQuad(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FStaticMeshData> CreateLine(const FVector& Start, const FVector& End, const FVector4& Color);
	static TUniquePtr<FStaticMeshData> CreatePlane(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	static TUniquePtr<FStaticMeshData> CreateAxis();

	static TUniquePtr<FStaticMeshData> CreateCone(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FStaticMeshData> CreateCylinder(float Radius, float Height, int Segments, const FVector4& Color);
	static TUniquePtr<FStaticMeshData> CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FStaticMeshData> CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color); // Cylinder + Cone 합성
	static TUniquePtr<FStaticMeshData> CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color);
	static TUniquePtr<FStaticMeshData> CreateCube(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));

	static TUniquePtr<FStaticMeshData> CreateSphere(float _radius, uint32 _numSlices, uint32 _numStacks, const FVector4& Color);

	static FStaticMeshData* GetMeshData(const FString& InName);

	static void CreateDefaultMeshDatas();

private:
	inline static TMap<FString, TUniquePtr<FStaticMeshData>> MeshDataMap;
};