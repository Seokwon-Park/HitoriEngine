#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FNameEntryId
{
	FNameEntryId() : Value(0) {}
	explicit FNameEntryId(uint32 InValue) : Value(InValue) {}

	uint32 ToUnstableInt() const { return Value; }
	bool operator==(const FNameEntryId& Other) const { return Value == Other.Value; }
	bool operator!=(const FNameEntryId& Other) const { return Value != Other.Value; }

private:
	uint32 Value;
};

class FName
{
public:
	FName() = default;
	FName(const char* Name);
	FName(const FString& Name);
	FName(FName Other, int32 InInternalNumber);
	FName(int32 InComparisonIndex, int32 InDisplayIndex, int32 InNumber);

	// 대소문자 무시 기준 동일 이름 + Number까지 비교
	bool operator==(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number; }
	bool operator!=(const FName& Other) const { return !(*this == Other); }


	int32 Compare(const FName& Other) const; // 알파벳 기준 비교
	
	bool IsNone() const { return ComparisonIndex == FNameEntryId(0) && Number == 0; }

	FString ToString() const;                // 이름 등록
	FString GetPlainNameString() const;      // Number 제외한 이름

	int32 GetNumber() const { return static_cast<int32>(Number); }

	FNameEntryId GetComparisonIndex() const { return ComparisonIndex; }
	FNameEntryId GetDisplayIndex() const { return DisplayIndex; }

private:
	FNameEntryId ComparisonIndex;  // 비교용
	FNameEntryId DisplayIndex;     // 표시용
	uint32 Number = 0;          // Cube_0 일 때, Number = 1이다. (0은 상태없음)
};

inline const FName NAME_None{};

struct FNameHash
{
	size_t operator()(const FName& Name) const noexcept
	{
		size_t Hash1 = std::hash<uint32>{}(Name.GetComparisonIndex().ToUnstableInt());
		size_t Hash2 = std::hash<int32>{}(Name.GetNumber());

		return Hash1 ^ (Hash2 << 1);
	}
};