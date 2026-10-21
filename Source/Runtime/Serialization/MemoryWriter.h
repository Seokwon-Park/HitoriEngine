#pragma once

#include <cstring>
#include <climits>
#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Core/EngineLog.h"
#include "Containers/Array.h"
#include "Serialization/MemoryArchive.h"

// 바이트 배열에 쓴다. 배열은 호출자가 소유하고 쓰는 만큼 늘어난다. UE의 FMemoryWriter.
// UE는 인덱스 크기별(32/64비트)로 템플릿을 두지만, 이 프로젝트의 TArray는 int32 고정이라 하나만 둔다.
class FMemoryWriter : public FMemoryArchive
{
public:
	// bSetOffset이 true면 기존 내용 뒤에 이어서 쓴다
	explicit FMemoryWriter(TArray<uint8>& InBytes, bool bSetOffset = false, const FName InArchiveName = NAME_None)
		: FMemoryArchive()
		, Bytes(InBytes)
		, ArchiveName(InArchiveName)
	{
		SetIsSaving(true);
		if (bSetOffset)
			Offset = InBytes.Num();
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num <= 0)
			return;

		const int64 RequiredSize = Offset + Num;
		if (RequiredSize > INT32_MAX)              // UE의 IndexSize 검사에 해당
		{
			HTR_LOG(Error, "{}: data larger than TArray can hold", GetArchiveName());
			SetError();
			return;
		}

		if (RequiredSize > Bytes.Num())
			Bytes.SetNum(static_cast<int32>(RequiredSize));

		std::memcpy(Bytes.GetData() + Offset, Data, static_cast<size_t>(Num));
		Offset += Num;
	}

	virtual FString GetArchiveName() const override
	{
		return ArchiveName != NAME_None ? ArchiveName.ToString() : FString("FMemoryWriter");
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

protected:
	TArray<uint8>& Bytes;
	const FName ArchiveName;
};