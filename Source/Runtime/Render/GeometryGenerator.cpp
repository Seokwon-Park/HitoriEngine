#include "EnginePCH.h"
#include "GeometryGenerator.h"

namespace
{
	FBox CalculateAABB(const TArray<FVertex>& Vertices)
	{
		FBox Box;

		if (Vertices.Num() == 0)
		{
			Box.Min = { 0.0f, 0.0f, 0.0f };
			Box.Max = { 0.0f, 0.0f, 0.0f };

			return Box;
		}

		FVector Min{ FLT_MAX, FLT_MAX, FLT_MAX };
		FVector Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const FVertex& Vertex : Vertices)
		{
			Min.X = fmin(Min.X, Vertex.Position.X);
			Min.Y = fmin(Min.Y, Vertex.Position.Y);
			Min.Z = fmin(Min.Z, Vertex.Position.Z);

			Max.X = fmax(Max.X, Vertex.Position.X);
			Max.Y = fmax(Max.Y, Vertex.Position.Y);
			Max.Z = fmax(Max.Z, Vertex.Position.Z);
		}
		Box.Min = Min;
		Box.Max = Max;

		return Box;
	}

	void CalculateNormals(TArray<FVertex>& Vertices, const TArray<uint32>& Indices)
	{
		for (FVertex& Vertex : Vertices)
		{
			Vertex.Normal = FVector();
		}
		for (int32 i = 0; i < Indices.Num(); i += 3)
		{
			uint32 IndexA = Indices[i];
			uint32 IndexB = Indices[i + 1];
			uint32 IndexC = Indices[i + 2];

			FVector A = Vertices[IndexA].Position;
			FVector B = Vertices[IndexB].Position;
			FVector C = Vertices[IndexC].Position;

			FVector Edge1 = B - A;
			FVector Edge2 = C - A;

			FVector Normal = FVector::Cross(Edge1, Edge2);

			Vertices[IndexA].Normal += Normal;
			Vertices[IndexB].Normal += Normal;
			Vertices[IndexC].Normal += Normal;
		}

		for (FVertex& Vertex : Vertices)
		{
			Vertex.Normal = Vertex.Normal.Normalized();
		}
	}
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreatePlane(float Size, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> PlaneMeshData = MakeUnique<FStaticMeshData>();

	float HalfSize = Size / 2.0f;

	TArray<FVector> positions =
	{
		FVector(-HalfSize, HalfSize, 0.0f),
		FVector(HalfSize, HalfSize, 0.0f),
		FVector(HalfSize, -HalfSize, 0.0f),
		FVector(-HalfSize, -HalfSize, 0.0f),
	};

	TArray<FVector2> uvs =
	{
		FVector2(0.0f, 0.0f),
		FVector2(1.0f, 0.0f),
		FVector2(1.0f, 1.0f),
		FVector2(0.0f, 1.0f),
	};

	for (int32 i = 0; i < positions.Num(); i++)
	{
		PlaneMeshData->Vertices.Add({ positions[i], FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), uvs[i], Color });
	}

	PlaneMeshData->Indices.Add(0);
	PlaneMeshData->Indices.Add(2);
	PlaneMeshData->Indices.Add(1);

	PlaneMeshData->Indices.Add(0);
	PlaneMeshData->Indices.Add(3);
	PlaneMeshData->Indices.Add(2);

	CalculateNormals(PlaneMeshData->Vertices, PlaneMeshData->Indices);
	PlaneMeshData->AABB = CalculateAABB(PlaneMeshData->Vertices);

	return PlaneMeshData;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateCone(float Radius, float Height, int Segments, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> ConeMeshData = MakeUnique<FStaticMeshData>();

	TArray<FVertex>& Vertices = ConeMeshData->Vertices;
	TArray<uint32>& Indices = ConeMeshData->Indices;

	Vertices.Add({ { 0.0f, 0.0f, Height }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(0.5f, 0.0f), Color });

	float Slice = PI * 2.0f / Segments;

	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = -Radius * cosf(angle);
		float y = -Radius * sinf(angle);
		float u = (float)i / Segments;

		Vertices.Add({ { x, y, 0.0f }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, 1.0f), Color });
	}

	for (int32 i = 0; i < Segments; i++)
	{
		uint32 bottomA = 1 + i;
		uint32 bottomB = 1 + i + 1;

		Indices.Add(0);
		Indices.Add(bottomA);
		Indices.Add(bottomB);
	}

	uint32 baseIndex = static_cast<uint32>(Vertices.Num());

	uint32 bottomCenterIdx = baseIndex;
	Vertices.Add({ { 0.0f, 0.0f, 0.0f }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(0.5f, 0.5f), Color });

	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		float u = 0.5f + 0.5f * cosf(angle);
		float v = 0.5f + 0.5f * sinf(angle);

		Vertices.Add({ { x, y, 0.0f }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, v), Color });
	}

	for (int32 i = 0; i < Segments; i++)
	{
		Indices.Add(bottomCenterIdx);
		Indices.Add(bottomRingStart + i + 1);
		Indices.Add(bottomRingStart + i);
	}

	CalculateNormals(ConeMeshData->Vertices, ConeMeshData->Indices);
	ConeMeshData->AABB = CalculateAABB(ConeMeshData->Vertices);

	return ConeMeshData;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateCylinder(float Radius, float Height, int32 Segments, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> CylinderMeshData = MakeUnique<FStaticMeshData>();

	TArray<FVertex>& Vertices = CylinderMeshData->Vertices;
	TArray<uint32>& Indices = CylinderMeshData->Indices;

	float Slice = PI * 2.0f / Segments;
	float HalfHeight = Height / 2.0f;

	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		float u = (float)i / Segments;

		Vertices.Add({ { x, y, -HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, 1.0f), Color });
		Vertices.Add({ { x, y,  HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, 0.0f), Color });
	}

	for (int32 i = 0; i < Segments; i++)
	{
		uint32 bottomA = i * 2;
		uint32 topA = i * 2 + 1;
		uint32 bottomB = (i + 1) * 2;
		uint32 topB = (i + 1) * 2 + 1;

		Indices.Add(bottomB); Indices.Add(topB); Indices.Add(topA);
		Indices.Add(bottomB); Indices.Add(topA); Indices.Add(bottomA);
	}

	uint32 baseIndex = static_cast<uint32>(Vertices.Num());

	uint32 bottomCenterIdx = baseIndex;
	Vertices.Add({ { 0.0f, 0.0f, -HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(0.5f, 0.5f), Color });

	uint32 topCenterIdx = baseIndex + 1;
	Vertices.Add({ { 0.0f, 0.0f, HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(0.5f, 0.5f), Color });

	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		float u = 0.5f + 0.5f * cosf(angle);
		float v = 0.5f + 0.5f * sinf(angle);

		Vertices.Add({ { x, y, -HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, v), Color });
	}

	uint32 topRingStart = static_cast<uint32>(Vertices.Num());
	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		float u = 0.5f + 0.5f * cosf(angle);
		float v = 0.5f + 0.5f * sinf(angle);

		Vertices.Add({ { x, y, HalfHeight }, FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 0.0f, 0.0f, 1.0f), FVector2(u, v), Color });
	}

	for (int32 i = 0; i < Segments; i++)
	{
		Indices.Add(bottomCenterIdx);
		Indices.Add(bottomRingStart + i + 1);
		Indices.Add(bottomRingStart + i);
	}

	for (int32 i = 0; i < Segments; i++)
	{
		Indices.Add(topCenterIdx);
		Indices.Add(topRingStart + i);
		Indices.Add(topRingStart + i + 1);
	}

	CalculateNormals(CylinderMeshData->Vertices, CylinderMeshData->Indices);
	CylinderMeshData->AABB = CalculateAABB(CylinderMeshData->Vertices);

	return CylinderMeshData;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> Arrow = CreateCylinder(BodyRadius, BodyHeight, Segments, Color);
	Arrow->Translate(FVector(0.0f, 0.0f, BodyHeight * 0.5f));   // 밑면을 0으로

	TUniquePtr<FStaticMeshData> Head = CreateCone(HeadRadius, HeadHeight, Segments, Color);
	Head->Translate(FVector(0.0f, 0.0f, BodyHeight));

	Arrow->Append(*Head);

	CalculateNormals(Arrow->Vertices, Arrow->Indices);
	Arrow->AABB = CalculateAABB(Arrow->Vertices);

	return Arrow;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> Arrow = CreateCylinder(BodyRadius, BodyLength, static_cast<int32>(Segments), Color);
	Arrow->Translate(FVector(0.0f, 0.0f, BodyLength * 0.5f));   // 밑면을 0으로

	TUniquePtr<FStaticMeshData> Head = CreateCube(HeadSize, Color);
	Head->Translate(FVector(0.0f, 0.0f, BodyLength + HeadSize * 0.5f));

	Arrow->Append(*Head);

	CalculateNormals(Arrow->Vertices, Arrow->Indices);
	Arrow->AABB = CalculateAABB(Arrow->Vertices);

	return Arrow;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> Data = MakeUnique<FStaticMeshData>();

	for (int i = 0; i <= Segments; ++i)
	{
		float theta = (float)i * 2.0f * PI / Segments;   // 큰 원을 도는 각도
		float cosTheta = cosf(theta);
		float sinTheta = sinf(theta);
		float u = (float)i / Segments;

		for (int j = 0; j <= TubeSegments; ++j)
		{
			float phi = (float)j * 2.0f * PI / TubeSegments;   // 단면 원을 도는 각도
			float cosPhi = cosf(phi);
			float sinPhi = sinf(phi);

			FVertex Vertex;
			Vertex.Position.X = (Radius + TubeRadius * cosPhi) * cosTheta;
			Vertex.Position.Y = (Radius + TubeRadius * cosPhi) * sinTheta;
			Vertex.Position.Z = TubeRadius * sinPhi;
			Vertex.UV = FVector2(u, (float)j / TubeSegments);
			Vertex.Color = Color;
			Data->Vertices.Add(Vertex);
		}
	}

	int stride = TubeSegments + 1;
	for (int i = 0; i < Segments; ++i)
	{
		for (int j = 0; j < TubeSegments; ++j)
		{
			int a = i * stride + j;
			int b = a + 1;
			int c = (i + 1) * stride + j;
			int d = c + 1;

			Data->Indices.Add(a);
			Data->Indices.Add(c);
			Data->Indices.Add(b);

			Data->Indices.Add(b);
			Data->Indices.Add(c);
			Data->Indices.Add(d);
		}
	}
	CalculateNormals(Data->Vertices, Data->Indices);
	Data->AABB = CalculateAABB(Data->Vertices);

	return Data;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateCube(float Size, const FVector4& Color)
{
	float HalfWidth = Size / 2.0f;
	float HalfHeight = Size / 2.0f;
	float HalfDepth = Size / 2.0f;

	TArray<FVector> positions =
	{
		//Front
		FVector(-HalfWidth, -HalfHeight, -HalfDepth),
		FVector(-HalfWidth, HalfHeight, -HalfDepth),
		FVector(HalfWidth, HalfHeight, -HalfDepth),
		FVector(HalfWidth, -HalfHeight, -HalfDepth),
		//Back
		FVector(-HalfWidth, -HalfHeight, HalfDepth),
		FVector(-HalfWidth, HalfHeight, HalfDepth),
		FVector(HalfWidth, HalfHeight, HalfDepth),
		FVector(HalfWidth, -HalfHeight, HalfDepth)
	};

	TArray<FVector> colors =
	{
		FVector(1.0f,0.0f,0.6f),//Front
		FVector(1.0f,0.0f,0.6f), //Back
		FVector(0.6f,1.0f,0.0f), //Top
		FVector(0.6f,1.0f,0.0f),//Bottom
		FVector(0.0f,0.6f,1.0f),//Left
		FVector(0.0f,0.6f,1.0f), //Right
	};

	TArray<TArray<uint32>> cubeFaces =
	{
		{ 0,1,2,3 }, //Front
		{ 7,6,5,4 }, //Back
		{ 1,5,6,2 }, //Top
		{ 4,0,3,7 }, //Bottom
		{ 4,5,1,0 }, //Left
		{ 3,2,6,7 }, //Right
	};

	TArray<FVector2> faceUVs =
	{
		FVector2(0.0f, 1.0f),
		FVector2(0.0f, 0.0f),
		FVector2(1.0f, 0.0f),
		FVector2(1.0f, 1.0f),
	};

	TUniquePtr<FStaticMeshData> CubeMeshData = MakeUnique<FStaticMeshData>();

	for (int i = 0; i < 6; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			FVertex Vertex;
			Vertex.Position = positions[cubeFaces[i][j]];
			Vertex.UV = faceUVs[j];
			Vertex.Color = FVector4(colors[i], 1.0f);
			CubeMeshData->Vertices.Add(Vertex);
		}

		uint32 BaseVertexIndex = i * 4;

		CubeMeshData->Indices.Add(BaseVertexIndex + 0);
		CubeMeshData->Indices.Add(BaseVertexIndex + 1);
		CubeMeshData->Indices.Add(BaseVertexIndex + 2);

		CubeMeshData->Indices.Add(BaseVertexIndex + 0);
		CubeMeshData->Indices.Add(BaseVertexIndex + 2);
		CubeMeshData->Indices.Add(BaseVertexIndex + 3);
	}

	CalculateNormals(CubeMeshData->Vertices, CubeMeshData->Indices);
	CubeMeshData->AABB = CalculateAABB(CubeMeshData->Vertices);

	return CubeMeshData;
}

TUniquePtr<FStaticMeshData> FGeometryGenerator::CreateSphere(float Radius, uint32 NumSlices, uint32 NumStacks, const FVector4& Color)
{
	TUniquePtr<FStaticMeshData> SphereMeshData = MakeUnique<FStaticMeshData>();

	const float SliceStep = PI * 2.0f / NumSlices;
	const float StackStep = PI / NumStacks;

	for (uint32 i = 0; i <= NumStacks; i++)
	{
		const float Phi = i * StackStep;
		const float z = Radius * cosf(Phi);
		const float r = Radius * sinf(Phi);

		for (uint32 j = 0; j <= NumSlices; j++)
		{
			const float Theta = j * SliceStep;

			FVertex Vertex;
			Vertex.Position = FVector(r * cosf(Theta), r * sinf(Theta), z);
			Vertex.UV = FVector2(float(j) / NumSlices, float(i) / NumStacks);
			Vertex.Color = Color;

			SphereMeshData->Vertices.Add(Vertex);
		}
	}

	for (uint32 i = 0; i < NumStacks; i++)
	{
		const uint32 offset = (NumSlices + 1) * i;
		for (uint32 j = 0; j < NumSlices; j++)
		{
			const uint32 TopL = offset + j;
			const uint32 TopR = offset + j + 1;
			const uint32 BotL = offset + NumSlices + 1 + j;
			const uint32 BotR = offset + NumSlices + 1 + j + 1;

			SphereMeshData->Indices.Add(TopL);
			SphereMeshData->Indices.Add(BotL);
			SphereMeshData->Indices.Add(BotR);

			SphereMeshData->Indices.Add(TopL);
			SphereMeshData->Indices.Add(BotR);
			SphereMeshData->Indices.Add(TopR);
		}
	}

	CalculateNormals(SphereMeshData->Vertices, SphereMeshData->Indices);
	SphereMeshData->AABB = CalculateAABB(SphereMeshData->Vertices);

	return SphereMeshData;
}

FStaticMeshData* FGeometryGenerator::GetMeshData(const FString& InName)
{
	TUniquePtr<FStaticMeshData>* MeshData = MeshDataMap.Find(InName);
	if (MeshData)
	{
		return (*MeshData).get();
	}
	return nullptr;
}

void FGeometryGenerator::CreateDefaultMeshDatas()
{
	MeshDataMap["Cone"] = CreateCone(1.0f, 1.0f, 20, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	MeshDataMap["Cube"] = CreateCube(1.0f);
	MeshDataMap["Cylinder"] = CreateCylinder(1.0f, 1.0f, 20, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	MeshDataMap["Sphere"] = CreateSphere(1.0f, 100, 50, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	MeshDataMap["Plane"] = CreatePlane(1.0f, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	MeshDataMap["Arrow"] = CreateArrow(0.1f, 1.0f, 0.2f, 0.5f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	MeshDataMap["Ring"] = CreateRing(1.0f, 0.03f, 32, 16, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	MeshDataMap["ScaleBar"] = CreateScaleBar(0.1f, 1.0f, 0.4f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	MeshDataMap["GizmoSphere"] = CreateSphere(0.2f, 100, 50, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
}
