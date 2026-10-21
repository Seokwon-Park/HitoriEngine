#pragma once

#include "StructuredArchiveSlotBase.h"
#include "StructuredArchiveNameHelpers.h"
#include "Core/Misc/Optional.h"
#include "Core/EngineString.h"

class FName;
class UObject;
class FStructuredArchiveSlot;
class FStructuredArchiveRecord;
class FStructuredArchiveArray;
class FStructuredArchiveStream;
class FStructuredArchiveMap;

//값 하나가 들어갈 자리 (필드, 배열 원소, 맵 항목)
class FStructuredArchiveSlot final : public FSlotBase
{
public:
	FStructuredArchiveRecord EnterRecord();
	FStructuredArchiveArray EnterArray(int32& Num);
	FStructuredArchiveStream EnterStream();
	FStructuredArchiveMap EnterMap(int32& Num);
	FStructuredArchiveSlot EnterAttribute(FArchiveFieldName AttributeName);
	TOptional<FStructuredArchiveSlot> TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenWriting);

	// 한 슬롯에는 값을 하나만 쓰므로 void 반환
	void operator<<(uint8& V);  
	void operator<<(uint16& V); 
	void operator<<(uint32& V); 
	void operator<<(uint64& V);
	void operator<<(int8& V);
	void operator<<(int16& V);  
	void operator<<(int32& V); 
	void operator<<(int64& V);
	void operator<<(float& V); 
	void operator<<(double& V); 
	void operator<<(bool& V);
	void operator<<(FString& V); 
	void operator<<(FName& V); 
	void operator<<(UObject*& V);
	void Serialize(void* Data, uint64 DataSize);

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveRecord;
	friend class FStructuredArchiveArray;
	friend class FStructuredArchiveStream;
	friend class FStructuredArchiveMap;
	FStructuredArchiveSlot(FStructuredArchive& InAr, int32 InDepth, FElementId InId) : FSlotBase(InAr, InDepth, InId) {}
};

// 이름 있는 필드의 묶음
class FStructuredArchiveRecord final : public FSlotBase
{
public:
	FStructuredArchiveSlot EnterField(FArchiveFieldName Name);
	FStructuredArchiveRecord EnterRecord(FArchiveFieldName Name);
	FStructuredArchiveArray EnterArray(FArchiveFieldName Name, int32& Num);
	FStructuredArchiveStream EnterStream(FArchiveFieldName Name);

	TOptional<FStructuredArchiveSlot> TryEnterField(FArchiveFieldName Name, bool bEnterWhenWriting);

	template <typename T>
	inline FStructuredArchiveRecord& operator<<(TNamedValue<T> Item)
	{
		EnterField(Item.Name) << Item.Value;
		return *this;
	}

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveSlot;
	FStructuredArchiveRecord(FStructuredArchive& InAr, int32 InDepth, FElementId InId) : FSlotBase(InAr, InDepth, InId) {}
};

// 개수가 정해진 목록
class FStructuredArchiveArray final : public FSlotBase
{
public:
	FStructuredArchiveSlot EnterElement();

	template <typename T>
	FStructuredArchiveArray& operator<<(T& Item) { EnterElement() << Item; return *this; }

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveSlot;
	FStructuredArchiveArray(FStructuredArchive& InAr, int32 InDepth, FElementId InId) : FSlotBase(InAr, InDepth, InId) {}
};

// 개수를 기록하지 않는 목록
class FStructuredArchiveStream final : public FSlotBase
{
public:
	FStructuredArchiveSlot EnterElement();

	template <typename T>
	FStructuredArchiveStream& operator<<(T& Item) { EnterElement() << Item; return *this; }

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveSlot;
	FStructuredArchiveStream(FStructuredArchive& InAr, int32 InDepth, FElementId InId) : FSlotBase(InAr, InDepth, InId) {}
};

// 키가 데이터인 목록
class FStructuredArchiveMap final : public FSlotBase
{
public:
	// 저장: Name이 키(입력) / 불러오기: 읽은 키(출력)
	FStructuredArchiveSlot EnterElement(FString& Name);

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveSlot;
	FStructuredArchiveMap(FStructuredArchive& InAr, int32 InDepth, FElementId InId) : FSlotBase(InAr, InDepth, InId) {}
};