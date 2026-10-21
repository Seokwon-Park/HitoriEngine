#include "EnginePCH.h"
#include "BinaryArchiveFormatter.h"

FBinaryArchiveFormatter::FBinaryArchiveFormatter(FArchive& InInner)
	:Inner(InInner)
{
}

bool FBinaryArchiveFormatter::HasDocumentTree() const
{
	return false;
}

FArchive& FBinaryArchiveFormatter::GetUnderlyingArchive()
{
	return Inner;
}

void FBinaryArchiveFormatter::EnterRecord()
{
}

void FBinaryArchiveFormatter::LeaveRecord()
{
}

 void FBinaryArchiveFormatter::EnterField(FArchiveFieldName Name)
{
}

void FBinaryArchiveFormatter::LeaveField()
{
}

 bool FBinaryArchiveFormatter::TryEnterField(FArchiveFieldName Name, bool bEnterWhenWriting)
{
	bool bValue = bEnterWhenWriting;
	Inner << bValue;
	if (bValue)
	{
		EnterField(Name);
	}
	return bValue;
}

void FBinaryArchiveFormatter::EnterArray(int32& NumElements)
{
	Inner << NumElements;
}

void FBinaryArchiveFormatter::LeaveArray()
{
}

void FBinaryArchiveFormatter::EnterArrayElement()
{
}

void FBinaryArchiveFormatter::LeaveArrayElement()
{
}

void FBinaryArchiveFormatter::EnterStream()
{
}

void FBinaryArchiveFormatter::LeaveStream()
{
}

void FBinaryArchiveFormatter::EnterStreamElement()
{
}

void FBinaryArchiveFormatter::LeaveStreamElement()
{
}

void FBinaryArchiveFormatter::EnterMap(int32& NumElements)
{
	Inner << NumElements;
}

void FBinaryArchiveFormatter::LeaveMap()
{
}

void FBinaryArchiveFormatter::EnterMapElement(FString& Name)
{
	Inner << Name;
}

void FBinaryArchiveFormatter::LeaveMapElement()
{
}

void FBinaryArchiveFormatter::EnterAttributedValue()
{
}

void FBinaryArchiveFormatter::EnterAttribute(FArchiveFieldName AttributeName)
{
}

void FBinaryArchiveFormatter::EnterAttributedValueValue()
{
}

bool FBinaryArchiveFormatter::TryEnterAttributedValueValue()
{
	return false;
}

void FBinaryArchiveFormatter::LeaveAttribute()
{
}

void FBinaryArchiveFormatter::LeaveAttributedValue()
{
}

bool FBinaryArchiveFormatter::TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenWriting)
{
	bool bValue = bEnterWhenWriting;
	Inner << bValue;
	if (bValue)
	{
		EnterAttribute(AttributeName);
	}
	return bValue;
}

void FBinaryArchiveFormatter::Serialize(uint8& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(uint16& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(uint32& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(uint64& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(int8& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(int16& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(int32& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(int64& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(float& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(double& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(bool& Value)
{
	Inner << Value;
}

//void FBinaryArchiveFormatter::Serialize(UTF32CHAR& Value)
//{
//	Inner << Value;
//}

void FBinaryArchiveFormatter::Serialize(FString& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(FName& Value)
{
	Inner << Value;
}

void FBinaryArchiveFormatter::Serialize(UObject*& Value)
{
	Inner << Value;
}

//#if WITH_VERSE_VM || defined(__INTELLISENSE__)
//void FBinaryArchiveFormatter::Serialize(Verse::VCell*& Value)
//{
//	Inner << Value;
//}
//#endif
//
//void FBinaryArchiveFormatter::Serialize(FText& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(FWeakObjectPtr& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(FSoftObjectPtr& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(FSoftObjectPath& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(FLazyObjectPtr& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(FObjectPtr& Value)
//{
//	Inner << Value;
//}
//
//void FBinaryArchiveFormatter::Serialize(TArray<uint8>& Data)
//{
//	Inner << Data;
//}

void FBinaryArchiveFormatter::Serialize(void* Data, uint64 DataSize)
{
	Inner.Serialize(Data, DataSize);
}