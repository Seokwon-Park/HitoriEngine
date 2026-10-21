#include "EnginePCH.h"

#include "StructuredArchive.h"
#include "StructuredArchiveSlots.h"

FStructuredArchiveSlot FStructuredArchive::Open()
{
	assert(CurrentScope.Num() == 0);
	assert(!RootElementId.IsValid());
	assert(!CurrentSlotElementId.IsValid());

	RootElementId = ElementIdGenerator.Generate();
	CurrentScope.Emplace(RootElementId, EElementType::Root);
	CurrentSlotElementId = ElementIdGenerator.Generate();

	return FSlot(*this, 0, CurrentSlotElementId);
}

void FStructuredArchive::Close()
{
	if (CurrentScope.Num() == 0)
		return;
	SetScope(FSlot(*this, 0, RootElementId));
	CurrentScope.RemoveLast();          // Root 제거 -> Close를 다시 불러도 안전
}

void FStructuredArchive::EnterSlot(const FSlotBase& Slot, bool bEnteringAttributedValue)
{
	int32 ParentDepth = Slot.Depth;
	FElementId ElementId = Slot.ElementId;

	if (ParentDepth + 1 < CurrentScope.Num() && CurrentScope[ParentDepth + 1].Id == ElementId && CurrentScope[ParentDepth + 1].Type == EElementType::AttributedValue)
	{
		SetScope(FSlot(*this, ParentDepth + 1, ElementId));
		Formatter.EnterAttributedValueValue();
	}
	else if (!bEnteringAttributedValue && Formatter.TryEnterAttributedValueValue())
	{
		int32 NewDepth = EnterSlotAsType(FSlotBase(*this, ParentDepth, ElementId), EElementType::AttributedValue);
		assert(NewDepth == ParentDepth + 1);
		FElementId AttributedValueId = CurrentScope[NewDepth].Id;
		SetScope(FSlot(*this, NewDepth, AttributedValueId));
	}
	else
	{
		assert(ElementId == CurrentSlotElementId);
		CurrentSlotElementId.Reset();
	}
}

int32 FStructuredArchive::EnterSlotAsType(const FSlotBase& Slot, EElementType Type)
{
	EnterSlot(Slot, Type == EElementType::AttributedValue);

	int32 NewSlotDepth = Slot.Depth + 1;

	if (NewSlotDepth < CurrentScope.Num() && CurrentScope[NewSlotDepth].Type == EElementType::AttributedValue)
	{
		++NewSlotDepth;
	}

	CurrentScope.Emplace(Slot.ElementId, Type);
	return NewSlotDepth;
}

void FStructuredArchive::LeaveSlot()
{
	switch (CurrentScope.Top().Type)
	{
		/*case FStructuredArchive::EElementType::Root:
			break;*/
	case EElementType::Record:
		Formatter.LeaveField();
		break;
	case EElementType::Array:
		Formatter.LeaveArrayElement();
		break;
	case EElementType::Stream:
		Formatter.LeaveStreamElement();
		break;
	case EElementType::Map:
		Formatter.LeaveMapElement();
		break;
	case EElementType::AttributedValue:
		Formatter.LeaveAttribute();
		break;
	default:
		break;
	}
}

void FStructuredArchive::SetScope(const FSlotBase& Slot)
{
	assert(Slot.Depth < CurrentScope.Num() && CurrentScope[Slot.Depth].Id == Slot.ElementId);
	assert(!CurrentSlotElementId.IsValid() || GetUnderlyingArchive().IsLoading());

	for (int32 CurrentDepth = CurrentScope.Num() - 1; CurrentDepth > Slot.Depth; CurrentDepth--)
	{
		// Leave the current element
		const FElement& Element = CurrentScope[CurrentDepth];
		switch (Element.Type)
		{
		case EElementType::Record:
			Formatter.LeaveRecord();
			break;
		case EElementType::Array:
			Formatter.LeaveArray();
			break;
		case EElementType::Stream:
			Formatter.LeaveStream();
			break;
		case EElementType::Map:
			Formatter.LeaveMap();
			break;
		case EElementType::AttributedValue:
			Formatter.LeaveAttributedValue();
			break;
		}

		CurrentScope.RemoveAt(CurrentDepth);

		LeaveSlot();
	}
	//CurrentScope.RemoveAt(Slot.Depth + 1, CurrentScope.Num() - (Slot.Depth + 1));

}

FArchive& FSlotBase::GetUnderlyingArchive() const
{
	return StructuredArchive.GetUnderlyingArchive();
}

//////////// FStructuredArchiveSlot ////////////

FStructuredArchiveRecord FStructuredArchiveSlot::EnterRecord()
{
	const int32 NewDepth = StructuredArchive.EnterSlotAsType(*this, FStructuredArchive::EElementType::Record);
	StructuredArchive.Formatter.EnterRecord();
	return FStructuredArchiveRecord(StructuredArchive, NewDepth, ElementId);
}

FStructuredArchiveArray FStructuredArchiveSlot::EnterArray(int32& Num)
{
	const int32 NewDepth = StructuredArchive.EnterSlotAsType(*this, FStructuredArchive::EElementType::Array);
	StructuredArchive.Formatter.EnterArray(Num);
	return FStructuredArchiveArray(StructuredArchive, NewDepth, ElementId);
}

FStructuredArchiveStream FStructuredArchiveSlot::EnterStream()
{
	const int32 NewDepth = StructuredArchive.EnterSlotAsType(*this, FStructuredArchive::EElementType::Stream);
	StructuredArchive.Formatter.EnterStream();
	return FStructuredArchiveStream(StructuredArchive, NewDepth, ElementId);
}

FStructuredArchiveMap FStructuredArchiveSlot::EnterMap(int32& Num)
{
	const int32 NewDepth = StructuredArchive.EnterSlotAsType(*this, FStructuredArchive::EElementType::Map);
	StructuredArchive.Formatter.EnterMap(Num);
	return FStructuredArchiveMap(StructuredArchive, NewDepth, ElementId);
}

// 값 쓰기: 슬롯 진입 확인 -> 포맷터에 값 전달 -> 슬롯에서 바로 나온다
void FStructuredArchiveSlot::operator<<(uint8& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(uint16& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(uint32& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(uint64& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(int8& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(int16& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(int32& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(int64& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(float& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(double& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(bool& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(FString& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(FName& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::operator<<(UObject*& V)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(V);
	StructuredArchive.LeaveSlot();
}

void FStructuredArchiveSlot::Serialize(void* Data, uint64 DataSize)
{
	StructuredArchive.EnterSlot(*this);
	StructuredArchive.Formatter.Serialize(Data, DataSize);
	StructuredArchive.LeaveSlot();
}

//////////// FStructuredArchiveRecord ////////////

FStructuredArchiveSlot FStructuredArchiveRecord::EnterField(FArchiveFieldName Name)
{
	StructuredArchive.SetScope(*this);
	StructuredArchive.CurrentSlotElementId = StructuredArchive.ElementIdGenerator.Generate();
	StructuredArchive.Formatter.EnterField(Name);
	return FStructuredArchiveSlot(StructuredArchive, Depth, StructuredArchive.CurrentSlotElementId);
}

FStructuredArchiveRecord FStructuredArchiveRecord::EnterRecord(FArchiveFieldName Name)
{
	return EnterField(Name).EnterRecord();
}

FStructuredArchiveArray FStructuredArchiveRecord::EnterArray(FArchiveFieldName Name, int32& Num)
{
	return EnterField(Name).EnterArray(Num);
}

FStructuredArchiveStream FStructuredArchiveRecord::EnterStream(FArchiveFieldName Name)
{
	return EnterField(Name).EnterStream();
}

TOptional<FStructuredArchiveSlot> FStructuredArchiveRecord::TryEnterField(FArchiveFieldName Name, bool bEnterWhenWriting)
{
	StructuredArchive.SetScope(*this);
	if (!StructuredArchive.Formatter.TryEnterField(Name, bEnterWhenWriting))
	{
		return TOptional<FStructuredArchiveSlot>();
	}
	StructuredArchive.CurrentSlotElementId = StructuredArchive.ElementIdGenerator.Generate();
	return FStructuredArchiveSlot(StructuredArchive, Depth, StructuredArchive.CurrentSlotElementId);
}

//////////// FStructuredArchiveArray ////////////

FStructuredArchiveSlot FStructuredArchiveArray::EnterElement()
{
	StructuredArchive.SetScope(*this);
	StructuredArchive.CurrentSlotElementId = StructuredArchive.ElementIdGenerator.Generate();
	StructuredArchive.Formatter.EnterArrayElement();
	return FStructuredArchiveSlot(StructuredArchive, Depth, StructuredArchive.CurrentSlotElementId);
}

//////////// FStructuredArchiveStream ////////////

FStructuredArchiveSlot FStructuredArchiveStream::EnterElement()
{
	StructuredArchive.SetScope(*this);
	StructuredArchive.CurrentSlotElementId = StructuredArchive.ElementIdGenerator.Generate();
	StructuredArchive.Formatter.EnterStreamElement();
	return FStructuredArchiveSlot(StructuredArchive, Depth, StructuredArchive.CurrentSlotElementId);
}

//////////// FStructuredArchiveMap ////////////

FStructuredArchiveSlot FStructuredArchiveMap::EnterElement(FString& Name)
{
	StructuredArchive.SetScope(*this);
	StructuredArchive.CurrentSlotElementId = StructuredArchive.ElementIdGenerator.Generate();
	StructuredArchive.Formatter.EnterMapElement(Name);
	return FStructuredArchiveSlot(StructuredArchive, Depth, StructuredArchive.CurrentSlotElementId);
}
