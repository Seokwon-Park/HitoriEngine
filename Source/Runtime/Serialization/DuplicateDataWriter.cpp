#include "EnginePCH.h"
#include "DuplicateDataWriter.h"

#include "UObject/UObjectGlobals.h"

FDuplicateDataWriter::FDuplicateDataWriter(FDuplicatedObjectAnnotation& InDuplicatedObjects, FLargeMemoryData& InObjectData, UObject* SourceObject, UObject* DestObject, EObjectFlags InFlagMask, EObjectFlags InApplyMask, EPropertyPortFlags InPortFlags)
	: DuplicatedObjectAnnotation(InDuplicatedObjects)
	, ObjectData(InObjectData)
	, Offset(0)
	, FlagMask(InFlagMask)
	, ApplyFlags(InApplyMask)
{
	SetIsSaving(true);
	SetIsPersistent(true);
	SetPortFlags(GetPortFlags() | EPropertyPortFlags::PPF_Duplicate | InPortFlags);

	AddDuplicate(SourceObject, DestObject);
}

FArchive& FDuplicateDataWriter::operator<<(FName& Name)
{
	uint32 ComparisonIndex = Name.GetComparisonIndex().ToUnstableInt();
	uint32 DisplayIndex = Name.GetDisplayIndex().ToUnstableInt();
	int32  Number = Name.GetNumber();
	Serialize(&ComparisonIndex, sizeof(ComparisonIndex));
	Serialize(&DisplayIndex, sizeof(DisplayIndex));
	Serialize(&Number, sizeof(Number));
	return *this;
}

FArchive& FDuplicateDataWriter::operator<<(UObject*& Object)
{
	GetDuplicatedObject(Object);
	Serialize(&Object, sizeof(UObject*));
	return *this;
}

//FArchive& FDuplicateDataWriter::operator<<(FLazyObjectPtr& LazyObjectPtr)
//{
//	return *this;
//}
//
//FArchive& FDuplicateDataWriter::operator<<(FObjectPtr& Object)
//{
//	return *this;
//}

void FDuplicateDataWriter::AddDuplicate(UObject* SourceObject, UObject* DupObject)
{
	//if (DupObject && !DupObject->IsTemplate())
	//{
	//	// Make sure the duplicated object is prepared to postload
	//	DupObject->SetFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
	//}

	DuplicatedObjectAnnotation.AddAnnotation(SourceObject, FDuplicatedObject(DupObject));
	UnserializedObjects.Add(SourceObject);
}

UObject* FDuplicateDataWriter::GetDuplicatedObject(UObject* Object, bool bCreateIfMissing)
{
	UObject* Result = nullptr;
	if (IsValid(Object))
	{
		// Check for an existing duplicate of the object.
		FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation(Object);
		if (!DupObjectInfo.IsDefault())
		{
			Result = DupObjectInfo.DuplicatedObject;
		}
		else if (bCreateIfMissing) //없으면 만든다.
		{
			// Check to see if the object's outer is being duplicated.
			UObject* DupOuter = GetDuplicatedObject(Object->GetOuter());
			if (DupOuter != nullptr)
			{
				// The object's outer is being duplicated, create a duplicate of this object.
				FStaticConstructObjectParameters Params(Object->GetClass());
				Params.Outer = DupOuter;
				Params.Name = Object->GetFName();
				Params.SetFlags = ApplyFlags | Object->GetMaskedFlags(FlagMask);
				Result = StaticConstructObject_Internal(Params);

				// If we assign external package to duplicated object, fetch the package
				AddDuplicate(Object, Result);
			}
		}
	}

	return Result;
}