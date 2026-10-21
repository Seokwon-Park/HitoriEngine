#include "EnginePCH.h"
#include "UObjectGlobals.h"

#include "UObjectHash.h"

#include "Class.h"

#include "Core/HAL/UnrealMemory.h"

#include "Serialization/DuplicatedObject.h"
#include "Serialization/LargeMemoryData.h"
#include "Serialization/DuplicateDataWriter.h"
#include "Serialization/DuplicateDataReader.h"


namespace
{
	FObjectDuplicationParameters InitStaticDuplicateObjectParams(UObject const* SourceObject, UObject* DestOuter,
		const FName DestName, EObjectFlags FlagMask, UClass* DestClass)
	{
		FObjectDuplicationParameters Parameters(const_cast<UObject*>(SourceObject), DestOuter);
		if (!DestName.IsNone())
		{
			Parameters.DestName = DestName;
		}
		else if (SourceObject->GetOuter() != DestOuter)
		{
			// try to keep the object name consistent if possible
			if (FindObjectFast<UObject>(DestOuter, SourceObject->GetFName()) == nullptr)
			{
				Parameters.DestName = SourceObject->GetFName();
			}
		}

		Parameters.DestClass = DestClass ? DestClass : SourceObject->GetClass();
		Parameters.FlagMask = FlagMask;
		// do not allow duplication of the Mark flags nor the HasExternalPackage flag
		//Parameters.FlagMask = FlagMask & ~(RF_MarkAsRootSet | RF_MarkAsNative | RF_HasExternalPackage);
		//Parameters.InternalFlagMask = InternalFlagsMask;
		//Parameters.DuplicateMode = DuplicateMode;

		Parameters.PortFlags = EPropertyPortFlags::PPF_DuplicateForPIE;

		return Parameters;
	}
}

//UObject* FObjectFactory::ConstructObject(UClass* Class, UObject* Outer, FName Name)
//{
//    if (!Class || !Class->Constructor)
//        return nullptr;
//
//    UObject* Object = Class->Constructor();
//    Object->ClassPrivate = Class;
//    HashObject(Object, Class);
//
//    Object->SetOuter(Outer);
//
//    Name = MakeUniqueObjectName(Class, Outer, Name);
//
//    Object->SetName(Name);
//
//    //HTR_LOG(Info, "Create {}", Class->Name);
//    //HTR_LOG(Info, "Total Allocation Bytes - {}", FEngineStatics::TotalAllocationBytes);
//    //HTR_LOG(Info, "Total Allocation Count - {}", FEngineStatics::TotalAllocationCount);
//
//    return Object;
//}

UObject* StaticConstructObject_Internal(const FStaticConstructObjectParameters& Params)
{
	bool bRecycled = false;
	const UClass* Class = Params.Class;
	if (!Class || !Class->Constructor)
		return nullptr;

	FName Name = Params.Name;
	if (Name == NAME_None)
	{
		Name = MakeUniqueObjectName(Params.Outer, Params.Class, Params.Name);
	}

	UObject* Object = StaticAllocateObject(Class, Params.Outer, Name, Params.SetFlags, true, &bRecycled);
	if (!Object) return nullptr;

	if (!bRecycled)
	{
		Class->Constructor(Object);
		Object->ClassPrivate = const_cast<UClass*>(Class);
		HashObject(Object, Class);
		Object->SetOuter(Params.Outer);
		Object->SetName(Name);
		if (Params.SetFlags != EObjectFlags::RF_NoFlags)
			Object->SetFlags(Params.SetFlags);
	}

	return Object;
}

FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName BaseName)
{
	if (!Class)
		return FName();

	// 이름을 따로 안 줬으면 Class 이름을 기본 이름으로 사용
	if (BaseName == NAME_None)
	{
		BaseName = FName(Class->Name);
	}

	const FString Base = BaseName.ToString();
	FName Candidate;
	do
	{
		Candidate = FName(Base + "_" + std::to_string(Class->ClassUnique++));
	} while (StaticFindObjectFast(nullptr, Outer, Candidate) != nullptr);

	return Candidate;
}

UObject* StaticDuplicateObject(UObject const* SourceObject, UObject* DestOuter, const FName DestName, EObjectFlags FlagMask, UClass* DestClass)
{
	FObjectDuplicationParameters Parameters = InitStaticDuplicateObjectParams(SourceObject, DestOuter, DestName, FlagMask, DestClass);
	return StaticDuplicateObjectEx(Parameters);
}

UObject* StaticDuplicateObjectEx(FObjectDuplicationParameters& Parameters)
{
	UObject* Source = Parameters.SourceObject;
	if (!Source) return nullptr;

	UObject* DupRootObject = Parameters.DuplicationSeed.FindRef(Parameters.SourceObject);
	if (DupRootObject == nullptr)
	{
		UClass* DestClass = Parameters.DestClass ? Parameters.DestClass : Parameters.SourceObject->GetClass();
		FStaticConstructObjectParameters Params(DestClass);
		Params.Outer = Parameters.DestOuter;
		Params.Name = Parameters.DestName;
		Params.SetFlags = Parameters.ApplyFlags | Parameters.SourceObject->GetMaskedFlags(Parameters.FlagMask);
		DupRootObject = StaticConstructObject_Internal(Params);
	}

	FDuplicatedObjectAnnotation DuplicatedObjectAnnotation;
	FLargeMemoryData ObjectData;
	TArray<UObject*> SerializedObjects;   // Reader가 같은 순서로 읽도록 기록

	{
		FDuplicateDataWriter Writer(DuplicatedObjectAnnotation, ObjectData,
			Source, DupRootObject, Parameters.FlagMask, Parameters.ApplyFlags, Parameters.PortFlags);

		// Seed는 대응표에만 등록 (직렬화하지 않음). 루트는 Writer 생성자에서 이미 등록됨
		for (const auto& [SeedSource, SeedDup] : Parameters.DuplicationSeed)
			if (SeedSource != Source)
				DuplicatedObjectAnnotation.AddAnnotation(SeedSource, FDuplicatedObject(SeedDup));

		// 직렬화 중에 GetDuplicatedObject가 큐에 객체를 계속 추가하므로 Num()을 매번 확인
		for (int32 i = 0; i < Writer.UnserializedObjects.Num(); ++i)
		{
			UObject* Obj = Writer.UnserializedObjects[i];
			SerializedObjects.Add(Obj);
			Obj->Serialize(Writer);
		}
	}

	// 3. 읽기
	{
		FDuplicateDataReader Reader(DuplicatedObjectAnnotation, ObjectData, Parameters.PortFlags, Parameters.DestOuter);
		for (UObject* Src : SerializedObjects)
		{
			UObject* Dup = DuplicatedObjectAnnotation.GetAnnotation(Src).DuplicatedObject;
			Dup->Serialize(Reader);
		}
		if (Reader.IsError())
			HTR_LOG(Error, "StaticDuplicateObjectEx: read failed for {}", Source->GetName());
	}

	const bool bDuplicateForPIE = HasFlag(Parameters.PortFlags, EPropertyPortFlags::PPF_DuplicateForPIE);
	for (UObject* Src : SerializedObjects)
	{
		if (UObject* Dup = DuplicatedObjectAnnotation.GetAnnotation(Src).DuplicatedObject)
			Dup->PostDuplicate(bDuplicateForPIE);
	}

	// 4. 결과
	if (Parameters.CreatedObjects)
	{
		for (UObject* Src : SerializedObjects)
			Parameters.CreatedObjects->Add(Src, DuplicatedObjectAnnotation.GetAnnotation(Src).DuplicatedObject);
	}

	return DupRootObject;
}

UObject* StaticFindObjectFast(UClass* Class, UObject* InOuter, FName InName, EFindObjectFlags Flags, EObjectFlags ExclusiveFlags)
{
	const bool bExactClass = HasFlag(Flags, EFindObjectFlags::ExactClass);

	for (UObject* Obj : GUObjectArray)            
	{
		// Outer와 이름이 같은지: 이 두 개가 같은 객체인가 조건
		if (!Obj || Obj->GetOuter() != InOuter || Obj->GetFName() != InName)
			continue;

		// 제외 플래그: 이 플래그를 가진 객체는 못 찾은 것으로 친다
		if (Obj->HasAnyFlags(ExclusiveFlags))
			continue;

		// 클래스 검사: Class가 nullptr이면 클래스 무관
		if (Class && (bExactClass ? Obj->GetClass() != Class : !Obj->GetClass()->IsChildOf(Class)))
			continue;

		return Obj;                               // 처음 찾은 객체 반환
	}
	return nullptr;
}

UObject* StaticAllocateObject(const UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags, bool bCanRecycleSubobjects, bool* bOutRecycledSubobject)
{
	assert(InClass && InClass->Constructor);

	UObject* Obj = nullptr;
	if (InName == NAME_None)
	{
		InName = MakeUniqueObjectName(InOuter, InClass);
	}
	else
	{
		Obj = StaticFindObjectFast(nullptr, InOuter, InName, EFindObjectFlags::ExactClass);
		if (Obj && !Obj->GetClass()->IsChildOf(InClass))
		{
			HTR_LOG(Error, "Cannot replace existing object {} of a different class", InName.ToString());
			return nullptr;
		}
	}
	bool bSubObject = false;
	if (!Obj)
	{
		//GUObjectAllocator.AllocateUObject
		Obj = static_cast<UObject*>(UObject::operator new(InClass->ClassSize));
	}
	else if (bCanRecycleSubobjects && Obj->HasAnyFlags(EObjectFlags::RF_DefaultSubObject))
	{
		bSubObject = true;                    // 재활용: 파괴하지 않음
		Obj->SetFlags(InFlags);
	}
	else
	{
		Obj->~UObject();                      // 교체: 같은 메모리에 다시 생성
	}

	if (!bSubObject)
		FMemory::Memzero(Obj, InClass->ClassSize);

	if (bOutRecycledSubobject)
		*bOutRecycledSubobject = bSubObject;

	return Obj;
}

FStaticConstructObjectParameters::FStaticConstructObjectParameters(const UClass* InClass)
	: Class(InClass)
{
}

FObjectDuplicationParameters::FObjectDuplicationParameters(UObject* InSourceObject, UObject* InDestOuter)
	: SourceObject(InSourceObject)
	, DestOuter(InDestOuter)
	, DestName(NAME_None)
	, FlagMask(EObjectFlags::RF_AllFlags)
	, ApplyFlags(EObjectFlags::RF_NoFlags)
	, PortFlags(EPropertyPortFlags::PPF_None)
{
}
