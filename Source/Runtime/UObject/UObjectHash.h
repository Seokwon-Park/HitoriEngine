#pragma once

class UObject;
class UClass;

void RegisterClass(UClass* Class);

UClass* FindClass(const FString& Name);

UObject* FindObjectWithOuter(const UObject* outer, const UClass* Class, FName Name);

void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIsIncludeDerivedClass = true);

void HashObject(UObject* Obj, const UClass* Class); 
void UnhashObject(UObject* Obj, const UClass* Class);

void GetDerivedClasses(const UClass* ClassToLookFor, TArray<UClass*>& Results, bool bRecursive = true);