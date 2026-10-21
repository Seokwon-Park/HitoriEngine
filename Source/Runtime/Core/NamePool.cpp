#include "EnginePCH.h"
#include "NamePool.h"

namespace UE::Core::Private
{
	FNamePool::FNamePool()
	{
		FNameEntryId NoneId(0);

		Entries.Add({ "None", NoneId });
		DisplayLookup.Add("None", NoneId);
		ComparisonLookup.Add("NONE", NoneId);
	}

	FNamePool& GetNamePool()
	{
		static FNamePool NamePool;
		return NamePool;
	}

	void ParseNumberFromName(FString& Name, int32& OutInternalNumber)
	{
		// 1. '_' 위치를 특정.
		size_t UnderscorePosition = Name.find_last_of('_');

		if (UnderscorePosition == FString::npos)
			return;

		if (UnderscorePosition + 1 >= Name.length())
			return;

		// 2. '_' 뒤쪽 숫자를 파싱.

		OutInternalNumber = 0;

		FString NumberString = Name.substr(UnderscorePosition + 1);

		// 3. 숫자인지 확인.
		for (char& Char : NumberString)
		{
			if (!std::isdigit(static_cast<unsigned char>(Char)))
				return;
		}

		// 4. 숫자로 변환한 뒤 1 크게 저장.
		int32 ExternalNumber = std::stoi(NumberString);

		OutInternalNumber = ExternalNumber + 1;

		// 5. '_'앞 부분 이름 파싱.
		Name = Name.substr(0, UnderscorePosition);
	}

	FString FNamePool::MakeComparisonKey(FStringView Name)
	{
		FString Result(Name.data(), Name.size());

		for (char& Ch : Result)
			Ch = static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));

		return Result;
	}

	FNameEntryId FNamePool::Store(FStringView Name)
	{
		if (Name.empty())
			return FNameEntryId();

		FString DisplayString(Name.data(), Name.size());

		// 대소문자까지 완전히 같은 이름이 이미 존재
		if (FNameEntryId* ExistingDisplayId = DisplayLookup.Find(DisplayString))
			return *ExistingDisplayId;

		// 대소문자를 무시하기 위한 비교용 문자열
		FString ComparisonString = MakeComparisonKey(Name);

		FNameEntryId ComparisonId;

		// 같은 이름이 이미 존재하면 기존 ComparisonId 재사용
		if (FNameEntryId* ExistingComparisonId = ComparisonLookup.Find(ComparisonString))
		{
			ComparisonId = *ExistingComparisonId;
		}
		else
		{
			// 처음 등장한 이름이면 지금 생성될 Entry가 Comparison 기준 Entry
			ComparisonId = FNameEntryId(static_cast<uint32>(Entries.Num()));
			ComparisonLookup.Add(ComparisonString, ComparisonId);
		}

		// 현재 표기법에 대한 DisplayId 생성
		FNameEntryId DisplayId(static_cast<uint32>(Entries.Num()));

		DisplayLookup.Add(DisplayString, DisplayId);
		Entries.Add({ DisplayString, ComparisonId });

		return DisplayId;
	}

	const FNameEntry& FNamePool::Resolve(FNameEntryId Id) const
	{
		return Entries[Id.ToUnstableInt()];
	}

}