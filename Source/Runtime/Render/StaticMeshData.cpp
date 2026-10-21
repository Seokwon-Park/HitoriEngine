#include "EnginePCH.h"
#include "StaticMeshData.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <format>
#include <vector>

namespace
{
	uint32 CountVertexCacheMisses(const uint32* Indices, uint32 IndexCount, uint32 CacheSize)
	{
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		uint32 Misses = 0;
		for (uint32 I = 0; I < IndexCount; ++I)
		{
			const uint32 Vertex = Indices[I];
			const auto It = std::find(Cache.begin(), Cache.end(), Vertex);
			if (It == Cache.end())
				++Misses;
			else
				Cache.erase(It);
			Cache.insert(Cache.begin(), Vertex);
			if (Cache.size() > CacheSize)
				Cache.pop_back();
		}
		return Misses;
	}
}

bool FStaticMeshData::Validate(FString& OutError) const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = std::format("invalid mesh (vertices {}, indices {})", VertexCount, IndexCount);
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= VertexCount)
		{
			OutError = std::format("index {} out of range (vertices {})", Index, VertexCount);
			return false;
		}
	}

	uint32 SectionIndexSum = 0;
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.StartIndex + Section.IndexCount > IndexCount)
		{
			OutError = std::format("section range {}+{} exceeds indices {}", Section.StartIndex, Section.IndexCount, IndexCount);
			return false;
		}
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
		{
			OutError = std::format("material slot {} out of range (slots {})", Section.MaterialSlotIndex, MaterialSlots.Num());
			return false;
		}
		SectionIndexSum += Section.IndexCount;
	}
	if (SectionIndexSum != IndexCount)
	{
		OutError = std::format("section index sum {} != indices {}", SectionIndexSum, IndexCount);
		return false;
	}

	if (!LODSourceVertices.IsEmpty() &&
		LODSourceVertices.Num() != Vertices.Num())
	{
		OutError = "LOD source vertex count does not match vertex count";
		return false;
	}

	return true;
}

void FStaticMeshData::OptimizeTriangleOrderForVertexCache()
{
	constexpr uint32 CacheSize = 32;
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.IndexCount < 6 || Section.StartIndex % 3 != 0 ||
			Section.IndexCount % 3 != 0 ||
			Section.StartIndex > static_cast<uint32>(Indices.Num()) ||
			Section.IndexCount > static_cast<uint32>(Indices.Num()) - Section.StartIndex)
			continue;

		const uint32 TriangleCount = Section.IndexCount / 3;
		const uint32* Source = Indices.GetData() + Section.StartIndex;
		std::vector<std::vector<uint32>> Adjacent(VertexCount);
		std::vector<uint32> Remaining(VertexCount, 0);
		for (uint32 T = 0; T < TriangleCount; ++T)
		{
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[T * 3 + Corner];
				if (V >= VertexCount) return;
				Adjacent[V].push_back(T);
				++Remaining[V];
			}
		}

		std::vector<uint8> Emitted(TriangleCount, 0);
		std::vector<uint32> Seen(TriangleCount, 0);
		std::vector<int32> CachePosition(VertexCount, -1);
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		std::vector<uint32> Reordered;
		Reordered.reserve(Section.IndexCount);
		uint32 NextTriangle = 0;
		uint32 Pass = 0;

		while (Reordered.size() < Section.IndexCount)
		{
			++Pass;
			uint32 Best = TriangleCount;
			float BestScore = -FLT_MAX;
			for (uint32 CachedVertex : Cache)
			{
				for (uint32 T : Adjacent[CachedVertex])
				{
					if (Emitted[T] || Seen[T] == Pass) continue;
					Seen[T] = Pass;
					float Score = 0.0f;
					for (uint32 Corner = 0; Corner < 3; ++Corner)
					{
						const uint32 V = Source[T * 3 + Corner];
						const int32 Position = CachePosition[V];
						if (Position >= 0)
						{
							Score += Position < 3 ? 0.75f :
								std::pow(static_cast<float>(CacheSize - Position) /
									static_cast<float>(CacheSize - 3), 1.5f);
						}
						Score += 2.0f / std::sqrt(static_cast<float>(Remaining[V]));
					}
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = T;
					}
				}
			}

			if (Best == TriangleCount)
			{
				while (NextTriangle < TriangleCount && Emitted[NextTriangle]) ++NextTriangle;
				Best = NextTriangle;
			}
			for (uint32 CachedVertex : Cache) CachePosition[CachedVertex] = -1;
			Emitted[Best] = 1;
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[Best * 3 + Corner];
				Reordered.push_back(V);
				--Remaining[V];
				const auto It = std::find(Cache.begin(), Cache.end(), V);
				if (It != Cache.end()) Cache.erase(It);
				Cache.insert(Cache.begin(), V);
				if (Cache.size() > CacheSize) Cache.pop_back();
			}
			for (uint32 I = 0; I < Cache.size(); ++I)
				CachePosition[Cache[I]] = static_cast<int32>(I);
		}

		const uint32 Old16 = CountVertexCacheMisses(Source, Section.IndexCount, 16);
		const uint32 Old32 = CountVertexCacheMisses(Source, Section.IndexCount, 32);
		const uint32 New16 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 16);
		const uint32 New32 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 32);
		if (New16 <= Old16 && New32 <= Old32 && (New16 < Old16 || New32 < Old32))
		{
			for (uint32 I = 0; I < Section.IndexCount; ++I)
				Indices[Section.StartIndex + I] = Reordered[I];
		}
	}
}

void FStaticMeshData::BuildTriangleBVH()
{
	TArray<FMeshTriangleElement> Elements;
	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		const FVector& V0 = Vertices[Indices[i]].Position;
		const FVector& V1 = Vertices[Indices[i + 1]].Position;
		const FVector& V2 = Vertices[Indices[i + 2]].Position;

		FMeshTriangleElement Element = {
			.V0 = V0,
			.Edge1 = V1 - V0,
			.Edge2 = V2 - V0,
		};

		Element.Bounds.Min.X = std::min({ V0.X, V1.X, V2.X });
		Element.Bounds.Min.Y = std::min({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Min.Z = std::min({ V0.Z, V1.Z, V2.Z });
		Element.Bounds.Max.X = std::max({ V0.X, V1.X, V2.X });
		Element.Bounds.Max.Y = std::max({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Max.Z = std::max({ V0.Z, V1.Z, V2.Z });
		Elements.Add(Element);
	}

	TriangleBVH.emplace([](const FMeshTriangleElement& Element) {
		return Element.Bounds;
	});

	TriangleBVH->Build(Elements);
}

void FStaticMeshData::BuildTangentSpace() {
	const int32 VertexCount{ Vertices.Num() };
	const int32 IndexCount{ Indices.Num() };
	constexpr float UVTolerance{ 1.0e-8f };
	constexpr float VectorToleranceSquared{ 1.0e-12f };

	TArray<FVector> TangentSums{};
	TArray<FVector> BitangentSums{};
	TangentSums.SetNum(VertexCount);
	BitangentSums.SetNum(VertexCount);
	// 삼각형마다 Tangent, Bitangent를 계산하고 각 정점에 합산
	for (int32 Index{ 0 }; Index + 2 < IndexCount; Index += 3) {
		const uint32 IndexA{ Indices[Index] };
		const uint32 IndexB{ Indices[Index + 1] };
		const uint32 IndexC{ Indices[Index + 2] };

		const FVertex& VertexA{ Vertices[IndexA] };
		const FVertex& VertexB{ Vertices[IndexB] };
		const FVertex& VertexC{ Vertices[IndexC] };

		const FVector Edge1{ VertexB.Position - VertexA.Position };
		const FVector Edge2{ VertexC.Position - VertexA.Position };
		const FVector2 DeltaUV1{ VertexB.UV - VertexA.UV };
		const FVector2 DeltaUV2{ VertexC.UV - VertexA.UV };

		const float Determinant{ DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X };
		if (std::abs(Determinant) <= UVTolerance) {
			continue;
		}

		const FVector Tangent{ (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) / Determinant };
		const FVector Bitangent{ (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) / Determinant };

		TangentSums[IndexA] += Tangent;
		TangentSums[IndexB] += Tangent;
		TangentSums[IndexC] += Tangent;

		BitangentSums[IndexA] += Bitangent;
		BitangentSums[IndexB] += Bitangent;
		BitangentSums[IndexC] += Bitangent;
	}

	// 각 정점마다 합산된 Tangent, Bitangent를 정규화하고 Handedness를 계산하여 최종 Tangent를 설정
	for (int32 Index{ 0 }; Index < VertexCount; ++Index) {
		FVertex& Vertex{ Vertices[Index] };
		const FVector Normal{ Vertex.Normal.GetSafeNormal(VectorToleranceSquared) };

		if (Normal.SizeSquared() <= VectorToleranceSquared) {
			Vertex.Tangent = FVector4{ 1.0f, 0.0f, 0.0f, 1.0f };
			continue;
		}

		FVector Tangent{ TangentSums[Index] - Normal * Normal.Dot(TangentSums[Index]) };

		if (Tangent.SizeSquared() <= VectorToleranceSquared) {
			const FVector Axis{ std::abs(Normal.X) < 0.9f ? FVector{ 1.0f, 0.0f, 0.0f } : FVector{ 0.0f, 1.0f, 0.0f } };
			Tangent = Axis - Normal * Normal.Dot(Axis);
		}

		Tangent = Tangent.GetSafeNormal(VectorToleranceSquared);

		const float Handedness{ Normal.Cross(Tangent).Dot(BitangentSums[Index]) < 0.0f ? -1.0f : 1.0f };
		Vertex.Tangent = FVector4{ Tangent, Handedness };
	}
}