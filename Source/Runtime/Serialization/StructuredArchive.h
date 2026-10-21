#pragma once

#include "Archive.h"
#include "StructuredArchiveFormatter.h"
#include "StructuredArchiveSlotBase.h"

using FArchiveFormatterType = FStructuredArchiveFormatter;



class FStructuredArchive
{
	friend class FStructuredArchiveSlot;
	friend class FStructuredArchiveRecord;
	friend class FStructuredArchiveArray;
	friend class FStructuredArchiveStream;
	friend class FStructuredArchiveMap;
public:
	using FSlot = FStructuredArchiveSlot;
	using FRecord = FStructuredArchiveRecord;
	using FArray = FStructuredArchiveArray;
	using FStream = FStructuredArchiveStream;
	using FMap = FStructuredArchiveMap;

	explicit FStructuredArchive(FArchiveFormatterType& InFormatter)
		:Formatter(InFormatter) {}
	~FStructuredArchive() { Close(); }

	FStructuredArchive(const FStructuredArchive&) = delete;
	FStructuredArchive& operator=(const FStructuredArchive&) = delete;

	FStructuredArchiveSlot Open();
	void Close();

	FArchive& GetUnderlyingArchive() const
	{
		return Formatter.GetUnderlyingArchive();
	}

	
private:
	enum class EElementType : uint8 { Root, Record, Array, Stream, Map, AttributedValue};
	struct FElement { FElementId Id; EElementType Type; };

	struct FIdGenerator
	{
		FElementId Generate()
		{
			return FElementId(NextId++);
		}

	private:
		uint32 NextId = 1;
	};

	void EnterSlot(const FSlotBase& Slot, bool bEnteringAttributedValue = false);
	int32 EnterSlotAsType(const FSlotBase& Slot, EElementType Type);
	void LeaveSlot();
	void SetScope(const FSlotBase& Slot);

	FArchiveFormatterType& Formatter;
	TArray<FElement> CurrentScope;
	FIdGenerator ElementIdGenerator;
	FElementId CurrentSlotElementId;
	FElementId RootElementId;
	uint32 NextId = 0;
};