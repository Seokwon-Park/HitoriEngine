#pragma once

#include "Containers/Array.h"
#include "Archive.h"
#include "Core/Types.h"
#include "Core/EngineString.h"
#include "StructuredArchiveNameHelpers.h"

class FArchive;
class FName;
class UObject;

enum class EArchiveValueType
{
	None,
	Record,
	Array,
	Stream,
	Map,
	Int8,
	Int16,
	Int32,
	Int64,
	UInt8,
	UInt16,
	UInt32,
	UInt64,
	Float,
	Double,
	Bool,
	String,
	Name,
	Object,
	Text,
	WeakObjectPtr,
	SoftObjectPtr,
	SoftObjectPath,
	LazyObjectPtr,
	RawData,
	AttributedValue,
	Attribute,
};

// FStructuredArchive가 실제 포맷(JSON, 바이너리 등)으로 읽고 쓰는 방법을 정의한다.
// 모든 Enter는 대응하는 Leave와 짝을 이뤄야 하고, 안쪽부터 역순으로 Leave한다.
// Serialize는 "현재 위치"(진입해 있는 필드나 원소)에 값 하나를 쓰거나 읽는다.
class FStructuredArchiveFormatter
{
public:
	virtual ~FStructuredArchiveFormatter();

	// 상태(IsLoading, 에러)와 실제 바이트 저장소를 제공하는 내부 FArchive를 반환한다.
	virtual FArchive& GetUnderlyingArchive() = 0;
	// 현재 위치부터 시작하는 독립된 읽기용 포맷터를 만든다. 문서 트리가 있는 입력 포맷터만 의미가 있고, 기본은 자기 자신.
	virtual FStructuredArchiveFormatter* CreateSubtreeReader() { return this; }

	// 키와 계층을 실제로 보존하는 포맷인지(JSON: true, 바이너리: false).
	// true면 필드를 이름으로 찾을 수 있어 순서가 달라도 되고, false면 저장한 순서 그대로 읽어야 한다.
	virtual bool HasDocumentTree() const = 0;

	// ── Record: 이름 있는 필드들의 묶음 (JSON의 { }) ──
	// 현재 위치를 Record로 시작/종료한다.
	virtual void EnterRecord() = 0;
	virtual void LeaveRecord() = 0;
	// Record 안의 Name 필드로 들어가고/나온다. 들어간 뒤 Serialize나 다른 Enter로 그 필드의 값을 다룬다.
	virtual void EnterField(FArchiveFieldName Name) = 0;
	virtual void LeaveField() = 0;
	// 있을 수도 없을 수도 있는 필드에 들어간다. true를 반환했을 때만 LeaveField를 호출한다.
	// 저장: bEnterWhenWriting이 true일 때만 들어가고 그 값을 반환한다.
	// 불러오기: 필드가 있으면 들어가고 true. (바이너리는 저장할 때 남긴 "있음/없음" 표시를 읽는다)
	virtual bool TryEnterField(FArchiveFieldName  Name, bool bEnterWhenWriting) = 0;

	// ── Array: 개수가 정해진 값 목록 (JSON의 [ ]) ──
	// 현재 위치를 Array로 시작한다. 저장할 때 NumElements는 입력(원소 수), 불러올 때는 출력(읽은 원소 수).
	virtual void EnterArray(int32& NumElements) = 0;
	virtual void LeaveArray() = 0;
	// 다음 원소 하나로 들어가고/나온다. 원소는 앞에서부터 순서대로 다룬다.
	virtual void EnterArrayElement() = 0;
	virtual void LeaveArrayElement() = 0;

	// ── Stream: 개수를 미리 기록하지 않는 값 목록 ──
	// Array와 같지만 원소 수를 저장하지 않는다. 읽는 쪽이 몇 개인지 따로 알고 있어야 한다.
	virtual void EnterStream() = 0;
	virtual void LeaveStream() = 0;
	virtual void EnterStreamElement() = 0;
	virtual void LeaveStreamElement() = 0;

	// ── Map: 키를 실행 중에 정하는 문자열-값 목록 (딕셔너리) ──
	// Record는 필드 이름이 코드에 고정되어 있고, Map은 키 자체가 데이터다.
	// 저장할 때 NumElements는 입력, 불러올 때는 출력.
	virtual void EnterMap(int32& NumElements) = 0;
	virtual void LeaveMap() = 0;
	// 다음 항목으로 들어간다. 저장할 때 Name은 이 항목의 키(입력), 불러올 때는 읽은 키(출력).
	virtual void EnterMapElement(FString& Name) = 0;
	virtual void LeaveMapElement() = 0;

	// ── AttributedValue: 본 값에 이름 있는 부가 정보(Attribute)를 붙인 값 ──
	// 현재 위치를 AttributedValue로 시작/종료한다.
	virtual void EnterAttributedValue() = 0;
	virtual void EnterAttribute(FArchiveFieldName AttributeName) = 0;
	// 본 값 쪽으로 들어간다.
	virtual void EnterAttributedValueValue() = 0;
	// EnterAttribute로 들어간 Attribute 하나에서 나온다.
	virtual void LeaveAttribute() = 0;
	virtual void LeaveAttributedValue() = 0;
	virtual bool TryEnterAttribute(FArchiveFieldName AttributeName, bool bEnterWhenWriting) = 0;
	// 현재 값이 AttributedValue 형태로 저장되어 있으면 본 값으로 들어가고 true. 일반 값이면 false.
	virtual bool TryEnterAttributedValueValue() = 0;

	// ── 값: 현재 위치에 값 하나를 쓰거나(저장) 읽는다(불러오기) ──
	virtual void Serialize(uint8& Value) = 0;
	virtual void Serialize(uint16& Value) = 0;
	virtual void Serialize(uint32& Value) = 0;
	virtual void Serialize(uint64& Value) = 0;
	virtual void Serialize(int8& Value) = 0;
	virtual void Serialize(int16& Value) = 0;
	virtual void Serialize(int32& Value) = 0;
	virtual void Serialize(int64& Value) = 0;
	virtual void Serialize(float& Value) = 0;
	virtual void Serialize(double& Value) = 0;
	virtual void Serialize(bool& Value) = 0;
	//virtual void Serialize(UTF32CHAR& Value) = 0;
	virtual void Serialize(FString& Value) = 0;
	// FName은 실행마다 바뀌는 이름 테이블 인덱스라 그대로 쓸 수 없다. 기록 방식(문자열 등)은 포맷터가 정한다.
	virtual void Serialize(FName& Value) = 0;
	// 객체 참조. 어떻게 기록하고 되살릴지(에셋 경로, 내부 FArchive에 위임 등)는 포맷터가 정한다.
	virtual void Serialize(UObject*& Value) = 0;
	//virtual void Serialize(FText& Value) = 0;
	//virtual void Serialize(struct FWeakObjectPtr& Value) = 0;
	//virtual void Serialize(struct FSoftObjectPtr& Value) = 0;
	//virtual void Serialize(struct FSoftObjectPath& Value) = 0;
	//virtual void Serialize(struct FLazyObjectPtr& Value) = 0;
	//virtual void Serialize(struct FObjectPtr& Value) = 0;
	//virtual void Serialize(TArray<uint8>& Value) = 0;
	// 해석하지 않는 원시 바이트 DataSize개를 쓰거나 읽는다.
	virtual void Serialize(void* Data, uint64 DataSize) = 0;
};