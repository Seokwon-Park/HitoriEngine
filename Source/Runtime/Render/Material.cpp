#include "EnginePCH.h"

#include "RenderCommand.h"

#include "Material.h"

#include "Asset/AssetManager.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/TypeSerializer.h"
#include "Serialization/MathSerialization.h"

namespace
{
	// 저장된 경로로 텍스처를 찾고, 아직 로드 전이면 로드한다
	UTexture2D* FindOrLoadTexture(const FString& Path)
	{
		if (UTexture2D* Texture = UAssetManager::GetAssetByPath<UTexture2D>(Path))
		{
			return Texture;
		}
		return UAssetManager::Get().LoadTexture(Path);
	}
}

UMaterial* UMaterial::CreateInstance(const UMaterial* Source)
{
	if (Source == nullptr)
	{
		return nullptr;
	}

	UMaterial* Instance = NewObject<UMaterial>();

	Instance->ParamLayout = Source->ParamLayout;
	Instance->Shader = Source->Shader;
	Instance->Textures = Source->Textures;
	Instance->BlendState = Source->BlendState;
	Instance->DepthStencilState = Source->DepthStencilState;
	Instance->SamplerState = Source->SamplerState;
	Instance->BaseColor = Source->BaseColor;
	Instance->SpecularColor = Source->SpecularColor;
	Instance->Shininess = Source->Shininess;
	Instance->UVScrollSpeed = Source->UVScrollSpeed;
	Instance->bFlipNormalY = Source->bFlipNormalY;
	Instance->PSOType = Source->PSOType;

	// ParamBuffer는 TUniquePtr라 복사할 수 없다.
	// 원본이 갖고 있으면 같은 크기로 새로 만들어 준다. 내용은 매 프레임 갱신되므로 옮기지 않는다.
	if (Source->ParamBuffer)
	{
		Instance->ParamBuffer = RenderCommand::CreateConstantBuffer(Source->ParamBuffer->GetBufferSize());
	}

	Instance->BaseColor = Source->BaseColor;
	Instance->bIsInstance = true;
	Instance->Parent = Source;

	return Instance;
}

const UMaterial* UMaterial::GetBaseAsset() const
{
	for (const UMaterial* Current = this; Current; Current = Current->Parent)
	{
		if (!Current->GetPath().empty())
		{
			return Current;
		}
	}
	return nullptr;
}

// 머티리얼 하나를 JSON으로.
// 에셋이면 경로만, 경로 없는 인스턴스면 "기준 에셋 + 텍스처 + 색"으로 풀어서 쓴다
json UMaterial::SaveMaterial(const UMaterial* Material)
{
	json Out;

	if (!Material->GetPath().empty())
	{
		Out["Asset"] = Material->GetPath();
		return Out;
	}

	const UMaterial* Base = Material->GetBaseAsset();
	Out["Base"] = Base ? Base->GetPath() : FString("DefaultMaterial");
	Out["PSOType"] = static_cast<uint8>(Material->PSOType);
	Out["BaseColor"] = Material->BaseColor;
	Out["SpecularColor"] = Material->SpecularColor;
	Out["Shininess"] = Material->Shininess;
	Out["FlipNormalY"] = Material->bFlipNormalY;
	Out["UVScrollSpeed"] = {
		Material->UVScrollSpeed.X,
		Material->UVScrollSpeed.Y
	};
	Out["SamplerState"] = Material->SamplerState == ESamplerState::LinearWrap ? "LinearWrap" : "LinearClamp";
	Out["BlendState"] = Material->BlendState == EBlendState::AlphaBlend ? "AlphaBlend" : "Opaque";

	json Textures = json::array();
	for (UTexture2D* Texture : Material->Textures)
	{
		if (Texture && !Texture->GetPath().empty())
			Textures.push_back(Texture->GetPath());
		else
			Textures.push_back(nullptr);
	}
	Out["Textures"] = Textures;

	return Out;
}

UMaterial* UMaterial::LoadMaterial(const json& In)
{
	if (In.contains("Asset"))
	{
		return UAssetManager::GetAssetByPath<UMaterial>(In["Asset"].get<FString>());
	}

	UMaterial* Base = nullptr;
	if (In.contains("Base"))
	{
		Base = UAssetManager::GetAssetByPath<UMaterial>(In["Base"].get<FString>());
	}
	if (!Base)
	{
		Base = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
	}

	UMaterial* Instance = UMaterial::CreateInstance(Base);
	if (!Instance)
	{
		return nullptr;
	}

	if (In.contains("BaseColor"))
	{
		In["BaseColor"].get_to(Instance->BaseColor);
	}
	if (In.contains("SpecularColor"))
	{
		In["SpecularColor"].get_to(Instance->SpecularColor);
	}
	if (In.contains("Shininess"))
	{
		Instance->Shininess = std::max(1.0f, In["Shininess"].get<float>());
	}
	if (In.contains("FlipNormalY"))
	{
		Instance->bFlipNormalY = In["FlipNormalY"].get<bool>();
	}

	if (In.contains("Textures"))
	{
		Instance->Textures.Reset();
		for (const json& TexturePath : In["Textures"])
		{
			Instance->Textures.Add(TexturePath.is_null() ? nullptr : FindOrLoadTexture(TexturePath.get<FString>()));
		}
	}
	if (In.contains("UVScrollSpeed"))
	{
		const auto& UV = In["UVScrollSpeed"];
		Instance->UVScrollSpeed = FVector2(UV[0].get<float>(), UV[1].get<float>());
	}
	if (In.contains("SamplerState"))
	{
		const FString State = In["SamplerState"].get<FString>();
		Instance->SamplerState = State == "LinearWrap" ? ESamplerState::LinearWrap : ESamplerState::LinearClamp;
	}
	if (In.contains("PSOType"))
	{
		Instance->PSOType = static_cast<EPSOType>(In["PSOType"].get<uint8>());
	}
	else if (In.contains("BlendState"))
	{
		const FString State = In["BlendState"].get<FString>();
		Instance->PSOType = (State == "AlphaBlend")
			? EPSOType::StaticMesh_Translucent
			: EPSOType::StaticMesh_Opaque;
		Instance->BlendState = State == "AlphaBlend" ? EBlendState::AlphaBlend : EBlendState::Opaque;
	}

	return Instance;
}

// Material.cpp
void UMaterial::SerializeMaterialReference(FStructuredArchive::FSlot Slot, UMaterial*& Material)
{
	FArchive& Ar = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	// ── 에셋 참조 ──
	// 저장과 불러오기가 같은 순서로 같은 함수를 부르게 TryEnterField를 쓴다 (바이너리 포맷에서도 맞도록)
	const bool bIsAsset = Ar.IsSaving() && Material && !Material->GetPath().empty();
	if (TOptional<FStructuredArchive::FSlot> AssetSlot = Record.TryEnterField("Asset", bIsAsset))
	{
		FString Path = Ar.IsSaving() ? Material->GetPath() : FString();
		AssetSlot.GetValue() << Path;
		if (Ar.IsLoading())
			Material = UAssetManager::GetAssetByPath<UMaterial>(Path);
		return;
	}

	// ── 인스턴스 ──
	const bool bIsInstance = Ar.IsSaving() && Material != nullptr;
	TOptional<FStructuredArchive::FSlot> BaseSlot = Record.TryEnterField("Base", bIsInstance);
	if (!BaseSlot.IsSet())
	{
		if (Ar.IsLoading())
			Material = nullptr;     // 빈 Record = 머티리얼 없음
		return;
	}

	FString BasePath;
	if (Ar.IsSaving())
	{
		const UMaterial* Base = Material->GetBaseAsset();
		BasePath = Base ? Base->GetPath() : FString("DefaultMaterial");
	}
	BaseSlot.GetValue() << BasePath;

	if (Ar.IsLoading())
	{
		UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>(BasePath);
		if (!Base)
		{
			HTR_LOG(Warning, "Load: base material '{}' not found, using DefaultMaterial", BasePath);
			Base = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
		}
		Material = CreateInstance(Base);   // 먼저 만들고
	}

	if (Material)
		Material->Serialize(Record.EnterRecord("Instance"));   // 그다음 값을 채운다
}

void UMaterial::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // "Properties" (리플렉션 프로퍼티. 지금은 없음)

	FArchive& Ar = Record.GetUnderlyingArchive();

	Record 	<< SA_VALUE("BaseColor", BaseColor)
			<< SA_VALUE("SpecularColor", SpecularColor)
			<< SA_VALUE("Shininess", Shininess)
			<< SA_VALUE("UVScrollSpeed", UVScrollSpeed)
			<< SA_VALUE("FlipNormalY", bFlipNormalY);

	// enum은 이름 문자열로 (UE 텍스트 포맷도 enum을 이름으로 쓴다)
	FString Sampler = SamplerState == ESamplerState::LinearWrap ? "LinearWrap" : "LinearClamp";
	Record << SA_VALUE("SamplerState", Sampler);
	if (Ar.IsLoading())
		SamplerState = Sampler == "LinearWrap" ? ESamplerState::LinearWrap : ESamplerState::LinearClamp;

	FString Blend = BlendState == EBlendState::AlphaBlend ? "AlphaBlend" : "Opaque";
	Record << SA_VALUE("BlendState", Blend);
	if (Ar.IsLoading())
		BlendState = Blend == "AlphaBlend" ? EBlendState::AlphaBlend : EBlendState::Opaque;

	uint8 PSOVal = static_cast<uint8>(PSOType);
	Record << SA_VALUE("PSOType", PSOVal);
	if (Ar.IsLoading())
		PSOType = static_cast<EPSOType>(PSOVal);

	// 텍스처: 경로 문자열 배열. 없는 칸은 빈 문자열
	int32 Num = Textures.Num();
	FStructuredArchive::FArray TextureArray = Record.EnterArray("Textures", Num);
	if (Ar.IsLoading())
		Textures.Reset();

	for (int32 i = 0; i < Num; ++i)
	{
		FString Path;
		if (Ar.IsSaving() && Textures[i])
			Path = Textures[i]->GetPath();

		TextureArray.EnterElement() << Path;

		if (Ar.IsLoading())
			Textures.Add(Path.empty() ? nullptr : FindOrLoadTexture(Path));
	}
}