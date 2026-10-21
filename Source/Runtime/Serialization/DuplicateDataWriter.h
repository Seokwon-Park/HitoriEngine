#pragma once

#include "ArchiveUObject.h"
#include "UObject/UObjectAnnotation.h"
#include "Serialization/DuplicatedObject.h"
#include "LargeMemoryData.h"

class FDuplicateDataWriter : public FArchiveUObject
{
public:
	FDuplicateDataWriter(
		FDuplicatedObjectAnnotation& InDuplicatedObjects,
		FLargeMemoryData& InObjectData,
		UObject* SourceObject,
		UObject* DestObject,
		EObjectFlags InFlagMask,
		EObjectFlags InApplyMask,
		EPropertyPortFlags InPortFlags);

	virtual FArchive& operator<<(FName& Value) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	//virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	//virtual FArchive& operator<<(FField*& Field) override;
	//virtual FArchive& operator<<(FObjectPtr& Object) override;

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (ObjectData.Write(Data, Offset, Num))
		{
			Offset += Num;
		}
		else
		{
			SetError();
		}
	}

	virtual void Seek(int64 InPos) override
	{
		Offset = InPos;
	}

	void AddDuplicate(UObject* SourceObject, UObject* DuplicateObject);

	virtual FString GetArchiveName() const { return "FDuplicateDataWriter"; }

	virtual int64 Tell() { return Offset; }
	virtual int64 TotalSize() { return ObjectData.GetSize(); }

	TArray<UObject*>	UnserializedObjects;

	UObject* GetDuplicatedObject(UObject* Object, bool bCreateIfMissing = true);
private:
	FDuplicatedObjectAnnotation& DuplicatedObjectAnnotation;
	FLargeMemoryData&						ObjectData;
	int64									Offset;
	EObjectFlags							FlagMask;
	EObjectFlags							ApplyFlags;
	//EInternalObjectFlags InternalFlagMask;
	//EInternalObjectFlags ApplyInternalFlags;
	//bool bAssignExternalPackages;
};