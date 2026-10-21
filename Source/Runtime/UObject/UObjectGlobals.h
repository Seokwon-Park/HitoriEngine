#pragma once

#include "Core/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/FindObjectFlags.h"
#include "Serialization/Archive.h"     // EPropertyPortFlags
#include "Containers/Map.h"

class UObject;
class UClass;

struct FStaticConstructObjectParameters
{
	const UClass* Class = nullptr;
	UObject* Outer = nullptr;
	FName Name;
	EObjectFlags SetFlags = EObjectFlags::RF_NoFlags;

	FStaticConstructObjectParameters(const UClass* InClass);
};

struct FObjectDuplicationParameters
{
	// 복제 대상
	UObject* SourceObject;

	// 복제 된 오브젝트가 가질 outer
	UObject* DestOuter;

	// 복제 된 오브젝트 이름
	FName			DestName;

	// Writer 생성자로 전달
	EObjectFlags	FlagMask;
	EObjectFlags	ApplyFlags;

	EPropertyPortFlags	PortFlags;

	UClass* DestClass = nullptr;

	// 복제할 때 쓰는 원본 복제본 대응표를 미리 채우는 용도. 이 맵에 있는 객체는 복제하지 않는다.
	// PersistentLevel 문제 해결용
	TMap<UObject*, UObject*> DuplicationSeed;

	//복제 중에 만든 객체 목록을 원본 -> 복제본으로 채워준다.
	TMap<UObject*, UObject*>* CreatedObjects = nullptr;

	FObjectDuplicationParameters(UObject* InSourceObject, UObject* InDestOuter);
};


UObject* StaticConstructObject_Internal(const FStaticConstructObjectParameters& Params);
UObject* DuplicateObject_Internal(UClass* Class, const UObject* SourceObject, UObject* Outer, const FName Name = NAME_None);
FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName BaseName = NAME_None);
UObject* StaticDuplicateObject(UObject const* SourceObject, UObject* DestOuter, const FName DestName = NAME_None, EObjectFlags FlagMask = EObjectFlags::RF_AllFlags, UClass* DestClass = nullptr);
UObject* StaticDuplicateObjectEx(FObjectDuplicationParameters& Parameters);
UObject* StaticFindObjectFast(UClass* Class, UObject* InOuter, FName InName, EFindObjectFlags Flags = EFindObjectFlags::None, EObjectFlags ExclusiveFlags = EObjectFlags::RF_NoFlags);

// 언리얼이 UObject를 할당하는 방식. 
// 여기서 memset을 모두 0으로 만들어 주는 것으로 초기화 변수 누락을 방지
UObject* StaticAllocateObject
(
	const UClass* InClass,
	UObject* InOuter,
	FName			InName,
	EObjectFlags	InFlags,
	bool bCanRecycleSubobjects,
	bool* bOutRecycledSubobject
);

template <class T>
T* NewObject
(
	UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = EObjectFlags::RF_NoFlags
)
{
	FStaticConstructObjectParameters Params(Class);
	Params.Outer = Outer;
	Params.Name = Name;
	Params.SetFlags = Flags;

	T* Result = static_cast<T*>(StaticConstructObject_Internal(Params));
	return Result;
};

template <class T>
T* NewObject(UObject* Outer = nullptr)
{
	FStaticConstructObjectParameters Params(T::StaticClass());
	Params.Outer = Outer;

	T* Result = static_cast<T*>(StaticConstructObject_Internal(Params));
	return Result;
}

template <class T>
T* NewObject(UObject* Outer, FName Name, EObjectFlags Flags = EObjectFlags::RF_NoFlags)
{
	FStaticConstructObjectParameters Params(T::StaticClass());
	Params.Outer = Outer;
	Params.Name = Name;
	Params.SetFlags = Flags;

	T* Result = static_cast<T*>(StaticConstructObject_Internal(Params));
	return Result;
}

template< class T >
inline T* FindObjectFast(UObject* Outer, FName Name, EFindObjectFlags Flags = EFindObjectFlags::None, EObjectFlags ExclusiveFlags = EObjectFlags::RF_NoFlags)
{
	return (T*)StaticFindObjectFast(T::StaticClass(), Outer, Name, Flags, ExclusiveFlags);
}



