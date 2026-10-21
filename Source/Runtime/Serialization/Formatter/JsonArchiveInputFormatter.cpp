#include "EnginePCH.h"
#include "JsonArchiveInputFormatter.h"

#include "Asset/RenderAsset.h"
#include "Asset/AssetManager.h"


FJsonArchiveInputFormatter::FJsonArchiveInputFormatter(FArchive& InInner)
    :Inner(InInner)
{
    assert(Inner.IsLoading());

    FString Text;
    Text.resize(static_cast<size_t>(Inner.TotalSize() - Inner.Tell()));
    Inner.Serialize(Text.data(), static_cast<int64>(Text.size()));

    Root = json::parse(Text, nullptr, false);
    if (Root.is_discarded())
    {
        HTR_LOG(Error, "Load: invalid json");
        Inner.SetError();
        Root = json::object(); 
    }
    ValueStack.Add(&Root);
}

FJsonArchiveInputFormatter::~FJsonArchiveInputFormatter()
{
}

const FJsonObject* FJsonArchiveInputFormatter::FindField(FArchiveFieldName Name)
{
    const FJsonObject* Object = ObjectStack.Last().JsonObject;
    if (!Object)
        return nullptr;
    auto It = Object->find(Name.Name);
    return It != Object->end() ? &*It : nullptr;
}


bool FJsonArchiveInputFormatter::HasDocumentTree() const
{
    return true;
}

void FJsonArchiveInputFormatter::EnterRecord()
{
    const FJsonObject* Value = Top();
    const bool bIsObject = Value && Value->is_object();

    if(Value && !bIsObject && !Value->is_null())
        HTR_LOG(Warning, "Load: expected object, got {}", Value->type_name());

    ObjectStack.Add({ bIsObject ? Value : nullptr, ValueStack.Num() });
}

void FJsonArchiveInputFormatter::LeaveRecord()
{
    assert(ValueStack.Num() == ObjectStack.Top().ValueCountOnCreation && "Record Fields ");
    ObjectStack.Pop();
}

void FJsonArchiveInputFormatter::EnterField(FArchiveFieldName Name)
{
    // UE는 없으면 crash. 여기서는 nullptr을 쌓아 이후 값 읽기가 기본값을 유지하게 한다.
    // Record 자체가 없을 때(Object == nullptr)는 이미 EnterRecord에서 드러나므로 필드마다 경고하지 않는다.
    const FJsonValue* Field = FindField(Name);
    if (!Field && ObjectStack.Last().JsonObject)
        HTR_LOG(Warning, "Load: missing field '{}', keeping default", Name.Name);

    ValueStack.Add(Field);
}

void FJsonArchiveInputFormatter::LeaveField()
{
    ValueStack.Pop();
}

bool FJsonArchiveInputFormatter::TryEnterField(FArchiveFieldName Name, bool bEnterWhenSaving)
{
    const FJsonObject* Field = FindField(Name.Name);
    if (!Field || Field->is_null())
        return false;
    ValueStack.Add(Field);
    return true;
}

void FJsonArchiveInputFormatter::EnterArray(int32& NumElements)
{
    const FJsonObject* Array = Top();
    NumElements = (Array && Array->is_array()) ? static_cast<int32>(Array->size()) : 0;

    for (int32 i = NumElements - 1; i >= 0; --i)
        ValueStack.Add(&(*Array)[i]);
    RemainingStack.Add(NumElements);
}

void FJsonArchiveInputFormatter::LeaveArray()
{
    for (int32 Remaining = RemainingStack.Top(); Remaining > 0; --Remaining)
        ValueStack.Pop();                  // 다 읽지 않은 원소 정리
    RemainingStack.Pop();
}

void FJsonArchiveInputFormatter::EnterArrayElement()
{
    if (RemainingStack.Top() <= 0)
    {
        HTR_LOG(Warning, "Load: array has fewer elements than expected");
        ValueStack.Add(nullptr);
    }
}

void FJsonArchiveInputFormatter::LeaveArrayElement()
{
    ValueStack.Pop();
    if (RemainingStack.Top() > 0)          // 실제 원소였을 때만 감소 (빈 값이면 0 그대로)
        --RemainingStack.Top();
}

void FJsonArchiveInputFormatter::EnterStream() { int32 Num = 0; EnterArray(Num); }
void FJsonArchiveInputFormatter::LeaveStream() { LeaveArray(); }
void FJsonArchiveInputFormatter::EnterStreamElement() { EnterArrayElement(); }
void FJsonArchiveInputFormatter::LeaveStreamElement() { LeaveArrayElement(); }

void FJsonArchiveInputFormatter::EnterMap(int32& NumElements)
{
    const FJsonObject* Object = Top();
    NumElements = 0;

    if (Object && Object->is_object())
    {
        TArray<std::pair<FString, const json*>> Items;
        for (auto It = Object->begin(); It != Object->end(); ++It)
            Items.Add({ It.key(), &It.value() });

        NumElements = Items.Num();
        for (int32 i = NumElements - 1; i >= 0; --i)
        {
            ValueStack.Add(Items[i].second);
            KeyStack.Add(Items[i].first);
        }
    }
    RemainingStack.Add(NumElements);
}

void FJsonArchiveInputFormatter::LeaveMap()
{
    for (int32 Remaining = RemainingStack.Top(); Remaining > 0; --Remaining)
    {
        ValueStack.Pop();
        KeyStack.Pop();
    }
    RemainingStack.Pop();
}

void FJsonArchiveInputFormatter::EnterMapElement(FString& Name)
{
    Name = KeyStack.Top();
}

void FJsonArchiveInputFormatter::LeaveMapElement()
{
    ValueStack.Pop();
    KeyStack.Pop();
    --RemainingStack.Top();
}


void FJsonArchiveInputFormatter::EnterAttributedValue()
{
}

void FJsonArchiveInputFormatter::EnterAttribute(FArchiveFieldName AttributeName)
{
}

void FJsonArchiveInputFormatter::EnterAttributedValueValue()
{
}

void FJsonArchiveInputFormatter::LeaveAttribute()
{
}

void FJsonArchiveInputFormatter::LeaveAttributedValue()
{
}

bool FJsonArchiveInputFormatter::TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenSavin)
{
    return false;
}

bool FJsonArchiveInputFormatter::TryEnterAttributedValueValue()
{
    return false;
}


void FJsonArchiveInputFormatter::Serialize(uint8& Value) { Read(Value, "uint8"); }
void FJsonArchiveInputFormatter::Serialize(uint16& Value) { Read(Value, "uint16"); }
void FJsonArchiveInputFormatter::Serialize(uint32& Value) { Read(Value, "uint32"); }
void FJsonArchiveInputFormatter::Serialize(uint64& Value) { Read(Value, "uint64"); }
void FJsonArchiveInputFormatter::Serialize(int8& Value) { Read(Value, "int8"); }
void FJsonArchiveInputFormatter::Serialize(int16& Value) { Read(Value, "int16"); }
void FJsonArchiveInputFormatter::Serialize(int32& Value) { Read(Value, "int32"); }
void FJsonArchiveInputFormatter::Serialize(int64& Value) { Read(Value, "int64"); }
void FJsonArchiveInputFormatter::Serialize(float& Value) { Read(Value, "float"); }
void FJsonArchiveInputFormatter::Serialize(double& Value) { Read(Value, "double"); }
void FJsonArchiveInputFormatter::Serialize(bool& Value) { Read(Value, "bool"); }
void FJsonArchiveInputFormatter::Serialize(FString& Value) { Read(Value, "string"); }

void FJsonArchiveInputFormatter::Serialize(FName& Value)
{
    FString String;
    if (Read(String, "string"))
        Value = FName(String);
}

void FJsonArchiveInputFormatter::Serialize(UObject*& Value)
{
    // null은 "되찾을 수 없던 값"이므로 기본값을 유지한다 (기존 Object.cpp와 같은 정책)
    FString AssetPath;
    if (!Read(AssetPath, "asset path"))
        return;

    if (URenderAsset* Asset = UAssetManager::GetAssetByPath<URenderAsset>(AssetPath))
        Value = Asset;
    else
        HTR_LOG(Warning, "Load: asset '{}' not found, keeping default", AssetPath);
}

void FJsonArchiveInputFormatter::Serialize(void* Data, uint64 DataSize)
{
    const json* Bytes = Top();
    if (!Bytes || !Bytes->is_array())
        return;

    uint8* Dst = static_cast<uint8*>(Data);
    const uint64 Count = std::min<uint64>(DataSize, Bytes->size());
    for (uint64 i = 0; i < Count; ++i)
        Dst[i] = (*Bytes)[i].get<uint8>();
}