#pragma once

#include "Core/Types.h"
#include "Core/EngineStatics.h"
#include "Core/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"

class UClass;
struct FProperty;
// Property Reflection

#define INVALID_OBJECT ((UObject*)-1)

#define REFLECT_START(ClassName) \
public: \
	inline static void RegisterProperties(UClass* InClass) \
	{

#define PROPERTY(PropertyName) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName));

#define PROPERTY_TYPE(PropertyName, PropertyType) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName), EPropertyType::##PropertyType);

// Flags: EPropertyFlags 조합 (예: CPF_VisibleAnywhere, CPF_EditAnywhere | CPF_Transient)
#define PROPERTY_FLAGS(PropertyName, Flags) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName), Flags);

#define PROPERTY_TYPE_FLAGS(PropertyName, PropertyType, Flags) \
    InClass->AddProperty<decltype(ThisClass::PropertyName)>(#PropertyName, offsetof(ThisClass, PropertyName), EPropertyType::##PropertyType, Flags);

#define REFLECT_END()\
	};\
private:

// 추상 클래스는 생성자를 등록하지 않는다.
// 템플릿이어야 버려진 if constexpr 분기의 new T()가 컴파일되지 않고,
// 멤버여야 private 생성자(싱글턴 등)에 접근할 수 있다.
#define DECLARE_CLASS(ClassName, SuperClassName)                        \
public:                                                                 \
    using Super = SuperClassName;                                       \
    using ThisClass = ClassName;		                                \
    template <typename T = ClassName>                                   \
	static UObject* InternalConstructInstance(void* Memory)				\
	{																	\
		if constexpr (std::is_abstract_v<T>) { return nullptr; }		\
		else { return new (Memory) T(); }								\
	}																	\
    static UClass* StaticClass()                                        \
    {                                                                   \
        static UClass c;                                                \
        static bool bIsInit = false;                                    \
        if (!bIsInit)                                                   \
        {                                                               \
            c.Name  = #ClassName;                                       \
            c.Super = Super::StaticClass();								\
            c.ClassSize = sizeof(ClassName);							\
			c.Constructor = std::is_abstract_v<ClassName> ? nullptr : &InternalConstructInstance<ClassName>;\
			if (&ClassName::RegisterProperties != &Super::RegisterProperties) \
			{															\
				ClassName::RegisterProperties(&c);						\
			}															\
			RegisterClass(&c);											\
            bIsInit = true;                                             \
        }                                                               \
        return &c;                                                      \
    }                                                                   \
    struct FAutoRegister {FAutoRegister() { ThisClass::StaticClass();}};\
    inline static FAutoRegister AutoRegister;							\
private:																

class UObject
{
	friend UObject* StaticConstructObject_Internal(const struct FStaticConstructObjectParameters& Params);
public:
	UObject();
	UObject(bool bRegister);
	virtual ~UObject();

	static UClass* StaticClass();
	UClass* GetClass() const { return ClassPrivate; }

	template <typename T>
	bool IsA() const { return IsA(T::StaticClass()); }
	bool IsA(const UClass* const Class) const;

	uint32 GetUUID() const { return ObjectUUID; }
	void SetUUID(uint32 Uid) { ObjectUUID = Uid; }

	const FName& GetFName() const { return Name; }         // FName 비교용
	FString GetName() const { return Name.ToString(); }    // Name 출력용

	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InOuter) { Outer = InOuter; }

	void SetName(const FName& InName) { Name = FName(InName); }

	inline static void RegisterProperties(UClass* InClass) {};

	inline void SetFlags(EObjectFlags NewFlags) { Flags |= NewFlags; }
	inline void ClearFlags(EObjectFlags FlagsToClear) { Flags &= ~FlagsToClear; }
	inline bool HasAnyFlags(EObjectFlags FlagsToCheck) const { return HasFlag(Flags, FlagsToCheck); }
	inline bool HasAllFlags(EObjectFlags FlagsToCheck) const { return (Flags & FlagsToCheck) == FlagsToCheck; }
	inline EObjectFlags GetFlags() const { return Flags; }
	EObjectFlags GetMaskedFlags(EObjectFlags Mask = EObjectFlags::RF_AllFlags) const{return EObjectFlags(GetFlags() & Mask);}

	virtual void Serialize(json& Handle, bool bIsLoading);
	virtual void Serialize(FArchive& Ar);
	virtual void Serialize(FStructuredArchive::FRecord Record);

	virtual void PostEditChangeProperty(const FProperty& Property) {};

	void* operator new(uint64 Size)
	{
		void* Ptr = malloc(Size);
		if (!Ptr)
			throw std::bad_alloc();

		FEngineStatics::TotalAllocationBytes += static_cast<uint64>(Size);
		FEngineStatics::TotalAllocationCount += 1;
		return Ptr;
	}

	void operator delete(void* Ptr, uint64 Size)
	{
		FEngineStatics::TotalAllocationBytes -= static_cast<uint64>(Size);
		FEngineStatics::TotalAllocationCount -= 1;
		free(Ptr);
	}

	void* operator new(uint64, void* Ptr) { return Ptr; }
	void operator delete(void*, void*) {}

	template <typename T>
	T* CreateDefaultSubobject(FName SubobjectName)
	{
		T* Subobject = NewObject<T>(this, SubobjectName, EObjectFlags::RF_DefaultSubObject);
		OnDefaultSubobjectCreated(Subobject);
		return Subobject;
	}

	virtual void PostDuplicate(bool bDuplicateForPIE) {}
protected:
	virtual void OnDefaultSubobjectCreated(UObject* Subobject) {}
private:
	void SerializeScriptProperties(FStructuredArchive::FSlot Slot);

	uint32 ObjectUUID;
	uint32 InternalIndex;

	FName Name = FName("None");
	UObject* Outer = nullptr;
	UClass* ClassPrivate = nullptr;

	EObjectFlags Flags = EObjectFlags::RF_NoFlags;

	bool bIsRegistered = true;
};

extern TArray<UObject*> GUObjectArray;

inline bool IsValid(const UObject* Test)
{
	return Test != nullptr;
}