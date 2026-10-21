#include "EnginePCH.h"
#include "UObjectHash.h"

#include "UObject/Object.h"
#include "UObject/Class.h"

//클래스에 해당하는 UObject들의 리스트(O(1)에 바로 가져옴)
static TMap<const UClass*, TSet<UObject*>>& GetClassToObjects() { static TMap<const UClass*, TSet<UObject*>> M; return M; }

//bIsIncludeDerivedClass가 켜졌을 때 자식 클래스 목록이 필요
static TMap<const UClass*, TSet<UClass*>>& GetClassToChildren() { static TMap<const UClass*, TSet<UClass*>> M; return M; }

static TMap<FString, UClass*>& GetClassMap()
{
	static TMap<FString, UClass* > Map;
	return Map;
}

void RegisterClass(UClass* Class)
{
	GetClassMap()[Class->Name] = Class;
	if (Class->Super)
	{
		GetClassToChildren()[Class->Super].Add(Class);
	}
}

UClass* FindClass(const FString& Name)
{
	UClass** Found = GetClassMap().Find(Name);
	return Found ? *Found : nullptr;
}

UObject* FindObjectWithOuter(const UObject* outer, const UClass* Class, FName Name)
{
	for (UObject* Obj : GUObjectArray)
	{
		if (Obj && Obj->GetOuter() == outer && Obj->GetFName() == Name && Obj->GetClass() == Class)
			return Obj;
	}
	return nullptr;
}

void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIsIncludeDerivedClass)
{
	if (!ClassToLookFor) return;

	Results.Reserve(Results.Num() + GUObjectArray.Num());
	//Class에 해당하는 Objects 리스트를 가져온다.
	if (const TSet<UObject*>* Objects = GetClassToObjects().Find(ClassToLookFor))
	{
		for (UObject* Obj : *Objects)
		{
			Results.Add(Obj);
		}
	}

	if (!bIsIncludeDerivedClass) return;

	//자식 클래스인 목록을 가져온다
	if (const TSet<UClass*>* ChildClasses = GetClassToChildren().Find(ClassToLookFor))
	{
		for (UClass* Child : *ChildClasses)
		{
			GetObjectsOfClass(Child, Results, bIsIncludeDerivedClass);
		}
	}
}

void HashObject(UObject* Obj, const UClass* Class)
{
	GetClassToObjects()[Class].Add(Obj);
}

void UnhashObject(UObject* Obj, const UClass* Class)
{
	if (TSet<UObject*>* Objects = GetClassToObjects().Find(Class))
		Objects->Remove(Obj);
}

void GetDerivedClasses(const UClass* ClassToLookFor, TArray<UClass*>& Results, bool bRecursive)
{
	if(const TSet<UClass*>* Children = GetClassToChildren().Find(ClassToLookFor))
		for (UClass* Child : *Children)
		{
			Results.Add(Child);
			if (bRecursive) GetDerivedClasses(Child, Results, true);
		}
}
