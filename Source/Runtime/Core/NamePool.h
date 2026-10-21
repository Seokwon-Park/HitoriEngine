#pragma once
#include "EnginePCH.h"
#include "Containers/StringView.h"

namespace UE::Core::Private
{
	struct FNameEntry
	{
		FString Name;
		FNameEntryId ComparisonId;
	};

	class FNamePool
	{
	public:
		FNamePool();

		FNameEntryId Store(FStringView Name);
		const FNameEntry& Resolve(FNameEntryId Id) const;

	private:
		static FString MakeComparisonKey(FStringView Name);

	private:
		TArray<FNameEntry> Entries;
		TMap<FString, FNameEntryId> DisplayLookup;
		TMap<FString, FNameEntryId> ComparisonLookup;
	};

	FNamePool& GetNamePool();
	void ParseNumberFromName(FString& Name, int32& OutInternalNumber);
}