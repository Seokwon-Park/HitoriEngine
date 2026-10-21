#include "EnginePCH.h"
#include "StaticMeshBake.h"

#include <cstring>
#include <fstream>
#include <type_traits>

#include "Render/StaticMeshData.h"
#include "Core/EngineLog.h"

static_assert(sizeof(FVertex) == 64);

// 바이트 그대로 읽고 쓰는 타입. 복사 생성자를 직접 작성하면 여기서 컴파일 오류가 난다
static_assert(std::is_trivially_copyable_v<FVertex>);
static_assert(std::is_trivially_copyable_v<FStaticMeshSection>);
static_assert(std::is_trivially_copyable_v<FVector4>);
static_assert(std::is_trivially_copyable_v<FBox>);

static_assert(std::is_trivially_copyable_v<FLODSourceVertex>);

namespace
{
	// FStaticMeshData, FVertex, FStaticMeshSection, FStaticMaterialSlot 구조가 바뀌면 올린다
	constexpr uint32 BakeVersion{ 5 };

	struct FBakeHeader
	{
		// 'S'taic 'M'esh 'B'a'K'e
		char Magic[4] = { 'S', 'M', 'B', 'K' };
		uint32 Version = BakeVersion;
	};

	// 숫자 || 숫자만 있는 구조체
	template<typename T>
	void WritePod(std::ofstream& Out, const T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "invalid plane old data");
		Out.write(reinterpret_cast<const char*>(&Value), sizeof(T));
	}

	template<typename T>
	bool ReadPod(std::ifstream& In, T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		In.read(reinterpret_cast<char*>(&Value), sizeof(T));
		return static_cast<bool>(In);
	}

	// 배열: 개수 + 원소 전체를 한 번에
	template<typename T>
	void WriteArray(std::ofstream& Out, const TArray<T>& Array)
	{
		const uint32 Count = static_cast<uint32>(Array.Num());
		WritePod(Out, Count);
		if (Count > 0)
		{
			Out.write(reinterpret_cast<const char*>(Array.GetData()), sizeof(T) * Count);
		}
	}

	template<typename T>
	bool ReadArray(std::ifstream& In, TArray<T>& Array, uint32 MaxCount)
	{
		uint32 Count = 0;
		if (!ReadPod(In, Count) || Count > MaxCount)
		{
			return false;
		}

		Array.SetNum(static_cast<int32>(Count));
		if (Count > 0)
		{
			In.read(reinterpret_cast<char*>(Array.GetData()), sizeof(T) * Count);
		}
		return static_cast<bool>(In);
	}

	// 길이 + 문자열
	void WriteString(std::ofstream& Out, const FString& Str)
	{
		const uint32 Length = static_cast<uint32>(Str.size());
		WritePod(Out, Length);
		Out.write(Str.data(), Length);
	}

	bool ReadString(std::ifstream& In, FString& Str)
	{
		uint32 Length = 0;
		if (!ReadPod(In, Length) || Length > 4096)
		{
			return false;
		}
		Str.resize(Length);
		In.read(Str.data(), Length);
		return static_cast<bool>(In);
	}
}

TUniquePtr<FStaticMeshData> FStaticMeshBake::ReadBaked(const FString& BinPath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	TUniquePtr<FStaticMeshData> Data = MakeUnique<FStaticMeshData>();
	uint32 MaxCount = 1000000;

	// 우리 형식이 맞는지, 같은 구조로 저장했는지 확인
	FBakeHeader Header;
	const FBakeHeader Expected;
	if (!ReadPod<FBakeHeader>(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-cooking", BinPath);
		return nullptr;
	}

	if (!ReadArray<FVertex>(In, Data->Vertices, MaxCount) ||
		!ReadArray<FLODSourceVertex>(In, Data->LODSourceVertices, MaxCount) ||
		!ReadArray<uint32>(In, Data->Indices, MaxCount) ||
		!ReadArray<FStaticMeshSection>(In, Data->Sections, MaxCount))
	{
		return nullptr;
	}

	uint32 MaterialSlotsNum = 0;
	if (!ReadPod<uint32>(In, MaterialSlotsNum))
	{
		return nullptr;
	}

	for (uint32 i = 0; i < MaterialSlotsNum; ++i)
	{
		FStaticMaterialSlot Slot{};
		if (!ReadString(In, Slot.Name) || !ReadPod<FVector4>(In, Slot.BaseColor) || !ReadString(In, Slot.DiffuseTexturePath) || !ReadString(In, Slot.NormalTexturePath)) {
			return nullptr;
		}
		Data->MaterialSlots.Add(std::move(Slot));
	}

	// AABB
	if (!ReadPod<FBox>(In, Data->AABB))
	{
		return nullptr;
	}

	// 바이트 수는 맞아도 내용이 깨졌을 수 있으므로 GPU에 올리기 전에 검사
	FString Error;
	if (!Data->Validate(Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-cooking", BinPath, Error);
		return nullptr;
	}

	HTR_LOG(Info, "[BAKE] {}: loaded", BinPath);
	return Data;
}

void FStaticMeshBake::WriteBaked(const FString& BinPath, const FStaticMeshData& Data)
{
	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	WritePod<FBakeHeader>(Out, FBakeHeader());
	WriteArray<FVertex>(Out, Data.Vertices);
	WriteArray<FLODSourceVertex>(Out, Data.LODSourceVertices);
	WriteArray<uint32>(Out, Data.Indices);
	WriteArray<FStaticMeshSection>(Out, Data.Sections);

	// MaterialSlots
	WritePod<uint32>(Out, Data.MaterialSlots.Num());
	for (const auto& Slot : Data.MaterialSlots)
	{
		WriteString(Out, Slot.Name);
		WritePod<FVector4>(Out, Slot.BaseColor);
		WriteString(Out, Slot.DiffuseTexturePath);
		WriteString(Out, Slot.NormalTexturePath);
	}

	// AABB
	WritePod<FBox>(Out, Data.AABB);

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}
