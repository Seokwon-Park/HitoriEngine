#include "EnginePCH.h"
#include "DuplicateDataReader.h"

FDuplicateDataReader::FDuplicateDataReader(FDuplicatedObjectAnnotation& InDuplicatedObjectAnnotation, const FLargeMemoryData& InObjectData, EPropertyPortFlags InPortFlags, UObject* InDestOuter)
	: DuplicatedObjectAnnotation(InDuplicatedObjectAnnotation)
	, ObjectData(InObjectData)
	, Offset(0)
{
	SetIsLoading(true);
	SetIsPersistent(true);
	SetPortFlags(GetPortFlags() | EPropertyPortFlags::PPF_Duplicate | InPortFlags);
}


FArchive& FDuplicateDataReader::operator<<(FName& N)
{
	uint32 ComparisonIndex = 0;
	uint32 DisplayIndex = 0;
	int32  Number = 0;
	Serialize(&ComparisonIndex, sizeof(ComparisonIndex));
	Serialize(&DisplayIndex, sizeof(DisplayIndex));
	Serialize(&Number, sizeof(Number));
	N = FName(ComparisonIndex, DisplayIndex, Number);
	return *this;
}

FArchive& FDuplicateDataReader::operator<<(UObject*& Object)
{
	UObject* SourceObject = Object;
	Serialize(&SourceObject, sizeof(UObject*));

	// 복사가 필요한 개체인가?
	FDuplicatedObject ObjectInfo = SourceObject ? DuplicatedObjectAnnotation.GetAnnotation(SourceObject) : FDuplicatedObject();
	if (!ObjectInfo.IsDefault())
	{
		Object = ObjectInfo.DuplicatedObject;
	}
	else
	{
		Object = SourceObject;
	}

	return *this;
}

//FArchive& FDuplicateDataReader::operator<<(FObjectPtr& Object)
//{
//	return *this;
//}
//
//FArchive& FDuplicateDataReader::operator<<(FLazyObjectPtr& LazyObjectPtr)
//{
//	return *this;
//}
//
//FArchive& FDuplicateDataReader::operator<<(FSoftObjectPath& SoftObjectPath)
//{
//	return *this;
//}

void FDuplicateDataReader::SerializeFail()
{
	SetError();
	/*HTR_LOG(Error, "FDuplicateDataReader Overread. SerializedObject = %ls SerializedProperty = %ls", *GetFullNameSafe(FUObjectThreadContext::Get().GetSerializeContext()->SerializedObject), *GetFullNameSafe(GetSerializedProperty()));*/
}