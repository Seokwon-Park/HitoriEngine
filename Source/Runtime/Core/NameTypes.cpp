#include "EnginePCH.h"
#include "NameTypes.h"
#include "NamePool.h"
#include "Containers/StringView.h"

FName::FName(const char* Name) 
	: FName(FString(Name))
{
}

FName::FName(const FString& Name)
{
	if (Name.empty())
		return;

	FString PlainName = Name;
	int32 InternalNumber = 0;

	UE::Core::Private::ParseNumberFromName(PlainName, InternalNumber);

	FNameEntryId DisplayId = UE::Core::Private::GetNamePool().Store(PlainName);
	const UE::Core::Private::FNameEntry& Entry = UE::Core::Private::GetNamePool().Resolve(DisplayId);

	ComparisonIndex = Entry.ComparisonId;
	DisplayIndex = DisplayId;
	Number = static_cast<uint32>(InternalNumber);
}


FName::FName(FName Other, int32 InNumber) 
	: ComparisonIndex(Other.ComparisonIndex),
	DisplayIndex(Other.DisplayIndex),
	Number(static_cast<uint32>(InNumber))
{
}

FName::FName(int32 InComparisonIndex, int32 InDisplayIndex, int32 InNumber)
	: ComparisonIndex(InComparisonIndex),
	DisplayIndex(InDisplayIndex),
	Number(static_cast<uint32>(InNumber))
{
}

int32 FName::Compare(const FName& Other) const
{
	if (ComparisonIndex == Other.ComparisonIndex)
	{
		if (Number < Other.Number)
			return -1;

		if (Number > Other.Number)
			return 1;

		return 0;
	}

	FString ThisString = GetPlainNameString();
	FString OtherString = Other.GetPlainNameString();

	if (ThisString < OtherString)
		return -1;

	if (ThisString > OtherString)
		return 1;

	return 0;
}

FString FName::ToString() const
{
	FString Result = GetPlainNameString();

	// 내부 Number 0 = 번호 없음
	if (Number > 0)
	{
		// 내부 1 -> 화면 _0
		Result += "_" + std::to_string(Number - 1);
	}

	return Result;
}

FString FName::GetPlainNameString() const
{
	return UE::Core::Private::GetNamePool().Resolve(DisplayIndex).Name;
}
