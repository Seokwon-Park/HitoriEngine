#pragma once

#include "ArchiveUObject.h"
#include "UObject/UObjectAnnotation.h"
#include "DuplicatedObject.h"
#include "LargeMemoryData.h"

class FDuplicateDataReader : public FArchiveUObject
{
public:
	FDuplicateDataReader(FDuplicatedObjectAnnotation& InDuplicatedObjectAnnotation, 
		const FLargeMemoryData& InObjectData, 
		EPropertyPortFlags InPortFlags,
		UObject* InDestOuter);
	virtual void Seek(int64 InPos) override
	{
		Offset = InPos;
	}

	virtual FString GetArchiveName() const { return "FDuplicateDataReader"; }

	virtual int64 Tell()
	{
		return Offset;
	}
	virtual int64 TotalSize()
	{
		return ObjectData.GetSize();
	}

private:
	virtual FArchive& operator<<(FName& N) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	//virtual FArchive& operator<<(FObjectPtr& Object) override;
	//virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	//virtual FArchive& operator<<(FSoftObjectPath& SoftObjectPath) override;

	void SerializeFail();

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (ObjectData.Read(Data, Offset, Num))
		{
			Offset += Num;
		}
		else
		{
			SerializeFail();
		}
	}

	FDuplicatedObjectAnnotation& DuplicatedObjectAnnotation;
	const FLargeMemoryData& ObjectData;
	int64 Offset;
};
