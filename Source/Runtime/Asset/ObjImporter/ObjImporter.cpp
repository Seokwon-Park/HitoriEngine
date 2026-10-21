#include "EnginePCH.h"
#include "ObjImporter.h"

#include <fstream>
#include <sstream>
#include <charconv>
#include <filesystem>

#include "Core/EngineLog.h"
#include "Render/StaticMeshData.h"
#include "Render/Vertex.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "Asset/ObjImporter/StaticMeshBake.h"

#include <algorithm>

namespace
{
	// OBJ (오른손, Y-Up) -> 엔진 (왼손, X-Forward, Y-Right, Z-Up)
	// 삼각형 winding도 함께 뒤집어야 한다
	FVector ToEngine(const FVector& V)
	{
		return FVector(-V.Z, V.X, V.Y);
	}

	bool ParseInt(FStringView Str, int32& Out)
	{
		if (Str.empty())
		{
			return false;
		}
		auto [Ptr, Ec] = std::from_chars(Str.data(), Str.data() + Str.size(), Out);
		return Ec == std::errc() && Ptr == Str.data() + Str.size();
	}

	bool ParseIndex(FStringView Token, FObjIndex& Out)
	{
		FStringView Parts[3];
		int32 PartCount = 0;
		size_t Start = 0;

		while (true)
		{
			if (PartCount == 3)
			{
				return false;
			}
			size_t Slash = Token.find('/', Start);
			Parts[PartCount++] = Token.substr(Start, Slash - Start);
			if (Slash == FStringView::npos)
			{
				break;
			}
			Start = Slash + 1;
		}

		// 마지막 칸이 비어 있으면 실패 ("1/", "1/2/", "1//")
		if (Parts[PartCount - 1].empty())
		{
			return false;
		}

		// 0 = 값 없음, -k = 마지막 k 번째
		FObjIndex Result{ 0, 0, 0 };

		if (!ParseInt(Parts[0], Result.PositionIndex) || Result.PositionIndex == 0)
		{
			return false;
		}
		if (PartCount >= 2 && !Parts[1].empty() &&
			(!ParseInt(Parts[1], Result.UVIndex) || Result.UVIndex == 0))
		{
			return false;
		}
		if (PartCount == 3 &&
			(!ParseInt(Parts[2], Result.NormalIndex) || Result.NormalIndex == 0))
		{
			return false;
		}

		Out = std::move(Result);
		return true;
	}

	bool TransformIndex(int32 Idx, int32 Count, int32& Out)
	{
		if (Idx > Count || Idx < -Count)
		{
			return false;
		}

		// 리터럴 '0'은 ParseIndex에서 처리됨 여기서 '0'은 속성 없음을 의미
		Out = (Idx < 0 ? Count + Idx : Idx - 1);
		return true;
	}

	bool TransformPath(const FString& BaseFilePath, FStringView RelativePath, FString& Out)
	{
		if (RelativePath.empty())
		{
			return false;
		}

		std::filesystem::path Relative(RelativePath);
		std::filesystem::path Result = Relative.is_absolute()
			? Relative
			: std::filesystem::path(BaseFilePath).parent_path() / Relative;

		Out = Result.lexically_normal().generic_string();
		return true;
	}

	bool ParseMaterial(const FString& Path, TArray<FObjMaterialInfo>& Out)
	{
		std::ifstream File(Path);
		if (!File.is_open())
		{
			HTR_LOG(Warning, "[MTL] {}: cannot open file", Path);
			return false;
		}

		TArray<FObjMaterialInfo> Parsed;
		FObjMaterialInfo MtlInfo;

		FString Line;
		int32 LineNumber = 0;
		while (std::getline(File, Line))
		{
			++LineNumber;
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.pop_back();
			}

			std::istringstream Stream(Line);
			FString Keyword;
			Stream >> Keyword;
			if (Keyword.empty() || Keyword[0] == '#')
			{
				continue; // 빈 줄 || 주석
			}

			if (Keyword == "newmtl")
			{
				if (!MtlInfo.Name.empty())
				{
					Parsed.Add(std::move(MtlInfo));
				}
				MtlInfo = FObjMaterialInfo();

				if (!(Stream >> MtlInfo.Name))
				{
					HTR_LOG(Warning, "[MTL] {}:{}: newmtl without name", Path, LineNumber);
					return false;
				}
			}
			else if (Keyword == "Kd")
			{
				if (!(Stream >> MtlInfo.Kd.X >> MtlInfo.Kd.Y >> MtlInfo.Kd.Z))
				{
					HTR_LOG(Warning, "[MTL] {}:{}: invalid Kd '{}'", Path, LineNumber, Line);
					return false;
				}
			}
			else if (Keyword == "map_Kd")
			{
				FString MapKd;
				if (!(Stream >> MapKd) || !TransformPath(Path, MapKd, MtlInfo.MapKd))
				{
					HTR_LOG(Warning, "[MTL] {}:{}: invalid map_Kd '{}'", Path, LineNumber, Line);
					return false;
				}
			}
			else if (Keyword == "d")
			{
				if (!(Stream >> MtlInfo.D))
				{
					HTR_LOG(Warning, "[MTL] {}:{}: invalid d '{}'", Path, LineNumber, Line);
					return false;
				}
			}
			else if (Keyword == "map_bump") {
				FString MapBump;
				if (!(Stream >> MapBump) || !TransformPath(Path, MapBump, MtlInfo.MapBump))
				{
					HTR_LOG(Warning, "[MTL] {}:{}: invalid map_bump '{}'", Path, LineNumber, Line);
					return false;
				}
			}
		}
		if (!MtlInfo.Name.empty())
		{
			Parsed.Add(std::move(MtlInfo));
		}

		for (FObjMaterialInfo& Info : Parsed)
		{
			Out.Add(std::move(Info));
		}
		return true;
	}

	FVector GetFaceNormal(const FObjFace& Face, const TArray<FVector>& Positions)
	{
		const FVector& A = Positions[Face.Indexes[0].PositionIndex];
		const FVector& B = Positions[Face.Indexes[1].PositionIndex];
		const FVector& C = Positions[Face.Indexes[2].PositionIndex];

		// 외적 길이 = 면적 x 2 이므로 정규화 (퇴화된 면은 0 벡터)
		return (B - A).Cross(C - A).Normalized();
	}
}

bool FObjImporter::ParseObj(const FString& Path, FObjInfo& Out)
{
	std::ifstream File(Path);
	if (!File.is_open())
	{
		HTR_LOG(Error, "[OBJ] {}: cannot open file", Path);
		return false;
	}

	FObjInfo Info;
	Info.Path = Path;

	FObjSection S;
	FString Line;
	int32 LineNumber = 0;
	while (std::getline(File, Line))
	{
		++LineNumber;
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}

		std::istringstream Stream(Line);
		FString Keyword;
		Stream >> Keyword;
		if (Keyword.empty() || Keyword[0] == '#')
		{
			continue; // 빈 줄 || 주석
		}

		if (Keyword == "v")
		{
			FVector P;
			if (!(Stream >> P.X >> P.Y >> P.Z))
			{
				HTR_LOG(Error, "[OBJ] {}:{}: invalid position '{}'", Path, LineNumber, Line);
				return false;
			}
			Info.Positions.Add(P);
		}
		else if (Keyword == "vt")
		{
			FVector2 UV;
			if (!(Stream >> UV.X >> UV.Y))
			{
				HTR_LOG(Error, "[OBJ] {}:{}: invalid uv '{}'", Path, LineNumber, Line);
				return false;
			}
			Info.UVs.Add(UV);
		}
		else if (Keyword == "vn")
		{
			FVector N;
			if (!(Stream >> N.X >> N.Y >> N.Z))
			{
				HTR_LOG(Error, "[OBJ] {}:{}: invalid normal '{}'", Path, LineNumber, Line);
				return false;
			}
			Info.Normals.Add(N);
		}
		else if (Keyword == "f")
		{
			FObjFace F;
			FString Token;
			while (Stream >> Token)
			{
				FObjIndex I;
				// 칸이 없거나 비어 있으면 0, 파일에 적힌 0은 실패
				if (!ParseIndex(Token, I))
				{
					HTR_LOG(Error, "[OBJ] {}:{}: invalid face token '{}'", Path, LineNumber, Token);
					return false;
				}
				if (!TransformIndex(I.PositionIndex, Info.Positions.Num(), I.PositionIndex) ||
					!TransformIndex(I.UVIndex, Info.UVs.Num(), I.UVIndex) ||
					!TransformIndex(I.NormalIndex, Info.Normals.Num(), I.NormalIndex))
				{
					HTR_LOG(Error, "[OBJ] {}:{}: face index out of range '{}' (v {}, vt {}, vn {})",
						Path, LineNumber, Token, Info.Positions.Num(), Info.UVs.Num(), Info.Normals.Num());
					return false;
				}
				if (I.NormalIndex == -1)
				{
					I.FaceIndex = Info.Faces.Num();
				}
				F.Indexes.Add(I);
			}
			if (F.Indexes.Num() < 3)
			{
				HTR_LOG(Error, "[OBJ] {}:{}: face needs at least 3 vertices (got {})", Path, LineNumber, F.Indexes.Num());
				return false;
			}
			Info.Faces.Add(F);
			S.Count++;
		}
		else if (Keyword == "usemtl")
		{
			FString Mtl;
			if (!(Stream >> Mtl))
			{
				HTR_LOG(Error, "[OBJ] {}:{}: usemtl without name", Path, LineNumber);
				return false;
			}

			if (S.Count > 0)
			{
				Info.Sections.Add(S);
			}

			S.Material = Mtl;
			S.Start = Info.Faces.Num();
			S.Count = 0;
		}
		else if (Keyword == "mtllib")
		{
			FString Mtl;
			while (Stream >> Mtl)
			{
				Info.Mtllibs.Add(Mtl);
			}
		}
	}
	// 마지막 Section 닫기
	if (S.Count > 0)
	{
		Info.Sections.Add(S);
	}

	// No Faces
	if (Info.Faces.IsEmpty())
	{
		HTR_LOG(Error, "[OBJ] {}: no faces", Path);
		return false;
	}

	// 기본 머티리얼
	FObjMaterialInfo DefaultMaterial;
	DefaultMaterial.Name = "Default";
	Info.MaterialInfos.Add(DefaultMaterial);

	// Material 파싱
	for (const auto& Mtl : Info.Mtllibs)
	{
		FString MtlPath;
		if (!TransformPath(Path, Mtl, MtlPath) ||
			!ParseMaterial(MtlPath, Info.MaterialInfos))
		{
			HTR_LOG(Warning, "[OBJ] {}: mtllib '{}' skipped", Path, Mtl);
		}
	}

	// Section.Material 채우기
	for (auto& Section : Info.Sections)
	{
		for (int32 i = 1; i < Info.MaterialInfos.Num(); ++i)
		{
			const auto& MtlInfo = Info.MaterialInfos[i];
			if (Section.Material == MtlInfo.Name)
			{
				Section.MaterialIndex = i;
				break;
			}
		}
		if (Section.MaterialIndex == 0 && !Section.Material.empty())
		{
			HTR_LOG(Warning, "[OBJ] {}: material '{}' not found, using Default", Path, Section.Material);
		}
	}

	Out = std::move(Info);
	return true;
}

bool FObjImporter::Cook(const FObjInfo& Raw, FStaticMeshData& Out)
{
	FStaticMeshData Cooked;

	// MaterialSlots 채우기
	for (const auto& RawMtl : Raw.MaterialInfos)
	{
		FStaticMaterialSlot CookedMtl{};
		CookedMtl.Name = RawMtl.Name;
		CookedMtl.BaseColor.Set(RawMtl.Kd, RawMtl.D);
		CookedMtl.DiffuseTexturePath = RawMtl.MapKd;
		CookedMtl.NormalTexturePath = RawMtl.MapBump;
		Cooked.MaterialSlots.Add(std::move(CookedMtl));
	}

	// Sections & Vertices & Indices
	TMap<FObjIndex, int32, FObjIndexHash> Cache;
	TArray<uint32> Corners; // 현재 면의 꼭짓점별 Cooked 정점 인덱스
	for (const auto& RawSection : Raw.Sections)
	{
		FStaticMeshSection CookedSection;
		CookedSection.StartIndex = static_cast<uint32>(Cooked.Indices.Num());
		CookedSection.MaterialSlotIndex = RawSection.MaterialIndex;
		// Section 내 Face 순회
		for (int32 FaceIndex = RawSection.Start; FaceIndex < RawSection.Start + RawSection.Count; ++FaceIndex)
		{
			const FObjFace& Face = Raw.Faces[FaceIndex];
			// vn 없는 꼭짓점이 쓸 face normal (면당 최초 1회만 계산)
			bool bHasFaceNormal = false;
			FVector FaceNormal;

			// Vertex Cache: 꼭짓점마다 한 번만 조회
			Corners.Reset();
			for (const auto& RawIndex : Face.Indexes)
			{
				if (const int32* Found = Cache.Find(RawIndex))
				{
					Corners.Add(static_cast<uint32>(*Found));
					continue;
				}

				const int32 NewIndex = Cooked.Vertices.Num();
				Cache[RawIndex] = NewIndex;
				Corners.Add(static_cast<uint32>(NewIndex));

				FVertex Vertex;
				Vertex.Position = ToEngine(Raw.Positions[RawIndex.PositionIndex]);
				if (RawIndex.NormalIndex >= 0)
				{
					Vertex.Normal = ToEngine(Raw.Normals[RawIndex.NormalIndex]);
				}
				else // face normal
				{
					if (!bHasFaceNormal)
					{
						FaceNormal = ToEngine(GetFaceNormal(Face, Raw.Positions));
						bHasFaceNormal = true;
					}
					Vertex.Normal = FaceNormal;
				}
				Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				if (RawIndex.UVIndex >= 0)
				{
					Vertex.UV = Raw.UVs[RawIndex.UVIndex];
					Vertex.UV.Y = 1.0f - Vertex.UV.Y;
				}
				Cooked.Vertices.Add(std::move(Vertex));
				Cooked.LODSourceVertices.Add({
					RawIndex.PositionIndex,
					RawIndex.UVIndex,
					RawIndex.NormalIndex
				});
			}

			// 삼각형 분할 (fan). OBJ는 CCW, 엔진은 CW가 앞면이라 (0, k, k+1) -> (0, k+1, k)
			for (int32 k = 1; k + 1 < Corners.Num(); ++k)
			{
				Cooked.Indices.Add(Corners[0]);
				Cooked.Indices.Add(Corners[k + 1]);
				Cooked.Indices.Add(Corners[k]);
			}
		}
		CookedSection.IndexCount = static_cast<uint32>(Cooked.Indices.Num()) - CookedSection.StartIndex;
		Cooked.Sections.Add(std::move(CookedSection));
	}

	// AABB (변환이 끝난 Position 기준)
	if (!Cooked.Vertices.IsEmpty())
	{
		FVector Min = Cooked.Vertices[0].Position;
		FVector Max = Min;
		for (const FVertex& Vertex : Cooked.Vertices)
		{
			Min.X = std::min(Min.X, Vertex.Position.X);
			Min.Y = std::min(Min.Y, Vertex.Position.Y);
			Min.Z = std::min(Min.Z, Vertex.Position.Z);
			Max.X = std::max(Max.X, Vertex.Position.X);
			Max.Y = std::max(Max.Y, Vertex.Position.Y);
			Max.Z = std::max(Max.Z, Vertex.Position.Z);
		}
		Cooked.AABB.Min = Min;
		Cooked.AABB.Max = Max;
	}

	FString Error;
	if (!Cooked.Validate(Error))
	{
		HTR_LOG(Error, "[OBJ] {}: {}", Raw.Path, Error);
		return false;
	}

	Out = std::move(Cooked);
	return true;
}

TUniquePtr<FStaticMeshData> FObjImporter::LoadStaticMeshData(const FString& Path)
{
	const FString BinPath = Path + ".bin";

	// .bin 파일 읽기
	if (TUniquePtr<FStaticMeshData> Baked = FStaticMeshBake::ReadBaked(BinPath))
	{
		return Baked;
	}

	FObjInfo Info;
	TUniquePtr<FStaticMeshData> Data = MakeUnique<FStaticMeshData>();

	if (!ParseObj(Path, Info) || !Cook(Info, *Data))
	{
		return nullptr;
	}

	FStaticMeshBake::WriteBaked(BinPath, *Data);
	return Data;
}

// 디버그용 출력
void FObjImporter::PrintObjInfo(const FObjInfo& ObjInfo)
{
	HTR_LOG(Info, "Path: {}", ObjInfo.Path);
	HTR_LOG(Info, "Positions Size: {}", ObjInfo.Positions.Num());
	HTR_LOG(Info, "UVs Size: {}", ObjInfo.UVs.Num());
	HTR_LOG(Info, "Normals Size: {}", ObjInfo.Normals.Num());
	HTR_LOG(Info, "Faces Size: {}", ObjInfo.Faces.Num());
	HTR_LOG(Info, "Sections Size: {}", ObjInfo.Sections.Num());
	HTR_LOG(Info, "Mtllibs Size: {}", ObjInfo.Mtllibs.Num());
	HTR_LOG(Info, "MaterialInfos Size: {}", ObjInfo.MaterialInfos.Num());
}

void FObjImporter::PrintSMD(const FStaticMeshData& SMD)
{
	HTR_LOG(Info, "Vertices Size: {}", SMD.Vertices.Num());
	HTR_LOG(Info, "Indices Size: {}", SMD.Indices.Num());
	HTR_LOG(Info, "Sections Size: {}", SMD.Sections.Num());
	HTR_LOG(Info, "MaterialSlots Size: {}", SMD.MaterialSlots.Num());
}
