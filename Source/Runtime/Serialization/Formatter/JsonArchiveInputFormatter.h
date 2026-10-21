#pragma once

#include "Serialization/StructuredArchiveFormatter.h"
#include "Json/Dom/JsonValue.h"
#include "Json/Dom/JsonObject.h"

class FJsonArchiveInputFormatter final : public FStructuredArchiveFormatter
{
public:
	FJsonArchiveInputFormatter(FJsonArchiveInputFormatter&&) = delete;
	FJsonArchiveInputFormatter(const FJsonArchiveInputFormatter&) = delete;
	FJsonArchiveInputFormatter& operator=(FJsonArchiveInputFormatter&&) = delete;
	FJsonArchiveInputFormatter& operator=(const FJsonArchiveInputFormatter&) = delete;

	explicit FJsonArchiveInputFormatter(FArchive& InInner);
	virtual ~FJsonArchiveInputFormatter();
	
	const FJsonObject* FindField(FArchiveFieldName Name);

	inline virtual FArchive& GetUnderlyingArchive() override { return Inner; }
	inline virtual FStructuredArchiveFormatter* CreateSubtreeReader() override { return this; }

	virtual bool HasDocumentTree() const override;

	virtual void EnterRecord() override;
	virtual void LeaveRecord() override;
	virtual void EnterField(FArchiveFieldName Name) override;
	virtual void LeaveField() override;
	virtual bool TryEnterField(FArchiveFieldName Name, bool bEnterWhenSaving) override;

	virtual void EnterArray(int32& NumElements) override;
	virtual void LeaveArray() override;
	virtual void EnterArrayElement() override;
	virtual void LeaveArrayElement() override;

	virtual void EnterStream() override;
	virtual void LeaveStream() override;
	virtual void EnterStreamElement() override;
	virtual void LeaveStreamElement() override;

	virtual void EnterMap(int32& NumElements) override;
	virtual void LeaveMap() override;
	virtual void EnterMapElement(FString& Name) override;
	virtual void LeaveMapElement() override;

	virtual void EnterAttributedValue() override;
	virtual void EnterAttribute(FArchiveFieldName AttributeName) override;
	virtual void EnterAttributedValueValue() override;
	virtual void LeaveAttribute() override;
	virtual void LeaveAttributedValue() override;
	virtual bool TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenSavin) override;
	virtual bool TryEnterAttributedValueValue() override;

	template <typename T>
	bool Read(T& Out, const char* TypeName)
	{
		const json* Value = Top();
		if (!Value || Value->is_null())
			return false;

		try
		{
			Value->get_to(Out);
			return true;
		}
		catch (const json::exception&)
		{
			HTR_LOG(Warning, "Load: expected {}, got {}, keeping default", TypeName, Value->type_name());
			return false;
		}
	}

	virtual void Serialize(uint8& Value) override;
	virtual void Serialize(uint16& Value) override;
	virtual void Serialize(uint32& Value) override;
	virtual void Serialize(uint64& Value) override;
	virtual void Serialize(int8& Value) override;
	virtual void Serialize(int16& Value) override;
	virtual void Serialize(int32& Value) override;
	virtual void Serialize(int64& Value) override;
	virtual void Serialize(float& Value) override;
	virtual void Serialize(double& Value) override;
	virtual void Serialize(bool& Value) override;
	//virtual void Serialize(UTF32CHAR& Value) override;
	virtual void Serialize(FString& Value) override;
	virtual void Serialize(FName& Value) override;
	virtual void Serialize(UObject*& Value) override;
	//virtual void Serialize(FText& Value) override;
	//virtual void Serialize(FWeakObjectPtr& Value) override;
	//virtual void Serialize(FSoftObjectPtr& Value) override;
	//virtual void Serialize(FSoftObjectPath& Value) override;
	//virtual void Serialize(FLazyObjectPtr& Value) override;
	//virtual void Serialize(FObjectPtr& Value) override;
	//virtual void Serialize(TArray<uint8>& Value) override;
	virtual void Serialize(void* Data, uint64 DataSize) override;

private:
	const FJsonValue* Top() { return ValueStack.Top(); }

	struct FObjectRecord
	{
		FObjectRecord(const FJsonObject* InJsonObject, int64 InValueCount)
			: JsonObject(InJsonObject)
			, ValueCountOnCreation(InValueCount)
		{

		}

		const FJsonObject* JsonObject;
		int64 ValueCountOnCreation;	// For debugging purposes, so we can ensure all values have been consumed
	};

	FArchive& Inner;
	FJsonValue Root;                    // ← 생성자에서 파싱한 트리
	TArray<const FJsonValue*> ValueStack;
	//지금 들어와 잇는 오브젝트
	TArray<FObjectRecord> ObjectStack;
	//배열에서 읽지 않은 원소 개수
	TArray<int32> RemainingStack;
	//맵의 키 순횡용
	TArray<FString> KeyStack;
};
