#include "EnginePCH.h"
#include "JsonArchiveOutputFormatter.h"

#include "Asset/RenderAsset.h"

FJsonArchiveOutputFormatter::FJsonArchiveOutputFormatter(FArchive& InInner)
	: Inner(InInner)
{
	assert(Inner.IsSaving());
	ValueStack.Add(&Root);
}

FJsonArchiveOutputFormatter::~FJsonArchiveOutputFormatter()
{
	// UE는 쓰는 즉시 Inner로 내보내지만, nlohmann은 트리를 다 만든 뒤 한 번에 내보낸다
	FString Text = Root.dump(4);
	Inner.Serialize(Text.data(), static_cast<int64>(Text.size()));
}

void FJsonArchiveOutputFormatter::EnterRecord()
{
	Top() = json::object();
}

void FJsonArchiveOutputFormatter::LeaveRecord()
{
}

void FJsonArchiveOutputFormatter::EnterField(FArchiveFieldName Name)
{
	json& Object = Top();
	assert(Object.is_object());
	ValueStack.Add(&Object[Name.Name]);      
}

void FJsonArchiveOutputFormatter::LeaveField()
{
	ValueStack.Pop();
}


bool FJsonArchiveOutputFormatter::TryEnterField(FArchiveFieldName Name, bool bEnterWhenSaving)
{
	if (bEnterWhenSaving)
		EnterField(Name);
	return bEnterWhenSaving;
}

void FJsonArchiveOutputFormatter::EnterArray(int32& NumElements)
{
	Top() = json::array();
}

void FJsonArchiveOutputFormatter::LeaveArray()
{
}

void FJsonArchiveOutputFormatter::EnterArrayElement()
{
	json& Array = Top();                     // 이전 원소는 이미 Leave됐으므로 Top은 배열
	Array.push_back(nullptr);
	ValueStack.Add(&Array.back());           // 다음 push_back 전에 항상 Pop되므로 안전

}

void FJsonArchiveOutputFormatter::LeaveArrayElement()
{
	ValueStack.Pop();
}

void FJsonArchiveOutputFormatter::EnterStream()
{
	Top() = json::array();
}

void FJsonArchiveOutputFormatter::LeaveStream()
{
}

void FJsonArchiveOutputFormatter::EnterStreamElement()
{
	EnterArrayElement();
}

void FJsonArchiveOutputFormatter::LeaveStreamElement()
{
	LeaveArrayElement();
}

void FJsonArchiveOutputFormatter::EnterMap(int32& NumElements)
{
	Top() = json::object();
}

void FJsonArchiveOutputFormatter::LeaveMap()
{
}

void FJsonArchiveOutputFormatter::EnterMapElement(FString& Name)
{
	ValueStack.Add(&Top()[Name]);
}

void FJsonArchiveOutputFormatter::LeaveMapElement()
{
	ValueStack.Pop();
}

void FJsonArchiveOutputFormatter::EnterAttributedValue()
{
}

void FJsonArchiveOutputFormatter::EnterAttribute(FArchiveFieldName AttributeName)
{
}

void FJsonArchiveOutputFormatter::EnterAttributedValueValue()
{
}

void FJsonArchiveOutputFormatter::LeaveAttribute()
{
}

void FJsonArchiveOutputFormatter::LeaveAttributedValue()
{
}

bool FJsonArchiveOutputFormatter::TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenSaving)
{
	return false;
}

bool FJsonArchiveOutputFormatter::TryEnterAttributedValueValue()
{
	return false;
}

// ── 값 ──
void FJsonArchiveOutputFormatter::Serialize(uint8& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(uint16& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(uint32& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(uint64& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(int8& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(int16& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(int32& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(int64& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(bool& Value) { Top() = Value; }
void FJsonArchiveOutputFormatter::Serialize(FString& Value) { Top() = Value; }

void FJsonArchiveOutputFormatter::Serialize(float& Value)
{
	// nlohmann은 NaN/무한대를 null로 출력한다 -> 불러올 때 기본값이 되므로 경고만 남긴다
	if (!std::isfinite(Value))
		HTR_LOG(Warning, "Save: non-finite float is written as null");
	Top() = Value;
}

void FJsonArchiveOutputFormatter::Serialize(double& Value)
{
	if (!std::isfinite(Value))
		HTR_LOG(Warning, "Save: non-finite double is written as null");
	Top() = Value;
}

void FJsonArchiveOutputFormatter::Serialize(FName& Value)
{
	Top() = Value.ToString();          // 실행마다 바뀌는 인덱스 대신 문자열
}

void FJsonArchiveOutputFormatter::Serialize(UObject*& Value)
{
	// 패키지가 없으므로 에셋 경로가 참조의 식별자 역할을 한다
	URenderAsset* Asset = Cast<URenderAsset>(Value);
	if (Asset && !Asset->GetPath().empty())
		Top() = Asset->GetPath();
	else
		Top() = nullptr;               // 경로 없는 객체는 되찾을 수 없음
}

void FJsonArchiveOutputFormatter::Serialize(void* Data, uint64 DataSize)
{
	json Bytes = json::array();
	const uint8* Src = static_cast<const uint8*>(Data);
	for (uint64 i = 0; i < DataSize; ++i)
		Bytes.push_back(Src[i]);
	Top() = std::move(Bytes);
}