#include "EnginePCH.h"
#include "Object.h"

#include "Core/EngineStatics.h"
#include "UObject/Class.h"

#include "Serialization/TypeSerializer.h"
#include "Serialization/MathSerialization.h"
#include "Serialization/Formatter/BinaryArchiveFormatter.h"
#include "Asset/AssetManager.h" //??? 의존성 제거 필요



TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	ObjectUUID = FEngineStatics::GetUUID();
	InternalIndex = GUObjectArray.Num();
	GUObjectArray.Add(this);
}

UObject::UObject(bool bRegister)
{
	bIsRegistered = bRegister;
}

UObject::~UObject()
{
	if (bIsRegistered)
	{
		if (ClassPrivate)
		{
			UnhashObject(this, ClassPrivate);
		}
		int32 LastIndex = GUObjectArray.Num() - 1;
		if (InternalIndex != LastIndex)
		{
			UObject* MovedObject = GUObjectArray[LastIndex];
			GUObjectArray[InternalIndex] = MovedObject;
			MovedObject->InternalIndex = InternalIndex; // 옮겨간 오브젝트 인덱스 갱신
		}
		GUObjectArray.RemoveLast();
	}
}

UClass* UObject::StaticClass()
{
	static UClass c;
	static bool bIsInit = false;
	if (!bIsInit)
	{
		c.Name = "Object";
		c.Super = nullptr;
		c.ClassSize = sizeof(UObject);
		c.Constructor = [](void* Memory) -> UObject*
			{
				return ::new (Memory) UObject();
			};
		bIsInit = true;
	}
	return &c;
}

bool UObject::IsA(const UClass* Class) const
{
	const UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

void UObject::Serialize(json& Handle, bool bIsLoading)
{
	for (UClass* c = GetClass(); c; c = c->Super)
	{
		for (FProperty Property : c->GetProperties())
		{
			if (Property.HasAnyFlags(CPF_Transient))
				continue;

			if (bIsLoading && !Handle.contains(Property.Name))
				continue;

			void* Ptr = reinterpret_cast<uint8*>(this) + Property.Offset;

			switch (Property.Type)
			{
			case EPropertyType::Float:
			{
				float& Value = *static_cast<float*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<float>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Int:
			{
				int32& Value = *static_cast<int32*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<int32>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::String:
			{
				FString& Value = *static_cast<FString*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<std::string>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Bool:
			{
				bool& Value = *static_cast<bool*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<bool>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Vector:
			{
				FVector& Value = *static_cast<FVector*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<FVector>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Vector4:
			{
				FVector4& Value = *static_cast<FVector4*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<FVector4>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Color:
			{
				// FColor(4바이트)는 RGBA를 묶은 uint32로 저장한다
				FColor& Value = *static_cast<FColor*>(Ptr);
				if (bIsLoading) Value.DWColor() = Handle[Property.Name].get<uint32>();
				else Handle[Property.Name] = Value.DWColor();
				break;
			}
			case EPropertyType::LinearColor:
			{
				FVector4& Value = *static_cast<FVector4*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<FVector4>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Transform:
			{
				FTransform& Value = *static_cast<FTransform*>(Ptr);
				if (bIsLoading) Value = Handle[Property.Name].get<FTransform>();
				else Handle[Property.Name] = Value;
				break;
			}
			case EPropertyType::Object:
			{
				UObject*& Value = *static_cast<UObject**>(Ptr);

				if (bIsLoading)
				{
					// null로 저장된 건 "되찾을 수 없는 값"이었으므로 기본값을 유지한다
					if (Handle[Property.Name].is_null())
					{
						break;
					}

					const FString AssetPath = Handle[Property.Name].get<FString>();
					URenderAsset* Asset = nullptr;

					if (Property.Class && Property.Class->IsChildOf(UStaticMesh::StaticClass()))
					{
						Asset = UAssetManager::GetAssetByPath<UStaticMesh>(AssetPath);
					}
					else
					{
						Asset = UAssetManager::GetAssetByPath<URenderAsset>(AssetPath);
					}

					// 못 찾으면 생성자가 넣어둔 기본값을 그대로 둔다.
					// (파일 하나가 없어져도 씬은 열리고, 액터가 사라지지 않고 기본 모양으로 보인다)
					if (!Asset || (Property.Class && !Asset->IsA(Property.Class)))
					{
						HTR_LOG(Warning, "Load: asset '{}' not found for {}, keeping default", AssetPath, Property.Name);
						break;
					}

					Value = Asset;
				}
				else
				{
					// 경로가 없는 에셋(런타임에 만든 인스턴스 등)은 되찾을 방법이 없으니 null로 쓴다
					URenderAsset* Asset = Cast<URenderAsset>(Value);
					if (Asset && !Asset->GetPath().empty())
						Handle[Property.Name] = Asset->GetPath();
					else
						Handle[Property.Name] = nullptr;
				}
				break;
			}
			default:
				break;
			}
		}
	}
}

void UObject::Serialize(FArchive& Ar)
{
	FBinaryArchiveFormatter Formatter(Ar);
	FStructuredArchive StructuredAr(Formatter);
	Serialize(StructuredAr.Open().EnterRecord());
	StructuredAr.Close();
}

void UObject::Serialize(FStructuredArchive::FRecord Record)
{
	SerializeScriptProperties(Record.EnterField("Properties"));
}

void UObject::SerializeScriptProperties(FStructuredArchive::FSlot Slot)
{
	FStructuredArchive::FRecord Properties = Slot.EnterRecord();

	const bool bIsLoading = Properties.GetUnderlyingArchive().IsLoading();

	for (UClass* c = GetClass(); c; c = c->Super)
	{
		for (const FProperty& Property : c->GetProperties())
		{
			TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField(Property.Name.c_str(), true);
			if (!Slot.IsSet()) continue;

			void* Ptr = reinterpret_cast<uint8*>(this) + Property.Offset;
			FStructuredArchive::FSlot Value = Slot.GetValue();

			switch (Property.Type)
			{
			case EPropertyType::Float:     Value << *static_cast<float*>(Ptr); break;
			case EPropertyType::Int:       Value << *static_cast<int32*>(Ptr); break;
			case EPropertyType::String:    Value << *static_cast<FString*>(Ptr); break;
			case EPropertyType::Bool:      Value << *static_cast<bool*>(Ptr); break;
			case EPropertyType::Vector:    Value << *static_cast<FVector*>(Ptr); break;
			case EPropertyType::Vector4:
			case EPropertyType::LinearColor: Value << *static_cast<FVector4*>(Ptr); break;
			case EPropertyType::Color:     Value << *static_cast<FColor*>(Ptr); break;
			case EPropertyType::Rotator:   Value << *static_cast<FRotator*>(Ptr); break;
			case EPropertyType::Transform: Value << *static_cast<FTransform*>(Ptr); break;
			case EPropertyType::Object:
			{
				UObject*& Ref = *static_cast<UObject**>(Ptr);
				UObject* Previous = Ref;
				Value << Ref;

				// 불러온 객체가 프로퍼티가 기대하는 클래스가 아니면 되돌린다 (기존 Asset->IsA(Property.Class) 검사)
				if (bIsLoading && Ref && Property.Class && !Ref->IsA(Property.Class))
				{
					HTR_LOG(Warning, "Load: '{}' is not a {}, keeping default", Property.Name, Property.Class->Name);
					Ref = Previous;
				}
				break;
			}
			default:
				// 슬롯을 열었으면 반드시 값을 써야 한다. 모르는 타입은 저장 시 null로 둔다.
				HTR_LOG(Warning, "Serialize: unsupported property type for '{}'", Property.Name);
				{ UObject* Null = nullptr; Value << Null; }
				break;
			}
		}
	}
}



