#pragma once

#include "MaterialInterface.h"
#include "RenderResourceManager.h"
#include "Render/RenderStates.h"
#include "Render/Texture2D.h"

class FShader;

enum class EMaterialParamLayout
{
	None,
	StaticMesh,
	ParticleSubUV
};

struct FStaticMeshMaterialParams
{
	FVector4 BaseColor;
	FVector2 UVOffset;
	FVector2 TextureFlags; // 텍스처 플래그 및 노멀 반전
};
static_assert(sizeof(FStaticMeshMaterialParams) == 32);

class UMaterial : public UMaterialInterface
{
	DECLARE_CLASS(UMaterial, UMaterialInterface)

public:
	UMaterial() :SortID(NextSortID++) {};
	virtual ~UMaterial() override = default;

	EMaterialParamLayout ParamLayout = EMaterialParamLayout::None;
	FShaderProgram* Shader = nullptr;
	TArray<UTexture2D*> Textures;
	TUniquePtr<FConstantBuffer> ParamBuffer;
	EPSOType PSOType = EPSOType::StaticMesh_Opaque;
	EBlendState BlendState = EBlendState::Opaque;
	EDepthStencilState DepthStencilState = EDepthStencilState::Default;
	ESamplerState SamplerState = ESamplerState::LinearClamp;

	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FVector SpecularColor = FVector(1.0f, 1.0f, 1.0f);
	float Shininess = 32.0f;
	FVector2 UVScrollSpeed = FVector2(0.0f, 0.0f);
	bool bFlipNormalY = false;

	bool bIsInstance = false;

	// 인스턴스가 복제된 원본. 인스턴스는 경로가 없어서, 저장할 때 원본을 따라가 기준 에셋을 찾는다
	const UMaterial* Parent = nullptr;

	// Source의 내용을 복사한 편집용 복제본을 만든다
	static UMaterial* CreateInstance(const UMaterial* Source);

	// 경로가 있는(=에셋으로 등록된) 가장 가까운 원본. 자신이 에셋이면 자신
	const UMaterial* GetBaseAsset() const;

	static json SaveMaterial(const UMaterial* Material);
	static UMaterial* LoadMaterial(const json& In);

	const uint16 SortID;

	using Super::Serialize;
	// 인스턴스의 값(색, 텍스처, 상태). 객체는 이미 만들어져 있어야 한다.
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	// 머티리얼 참조 하나를 Slot에 읽고 쓴다.
	// 에셋이면 경로만, 인스턴스면 기준 에셋 + 값을 그 자리에 쓰고, 불러올 때 인스턴스를 새로 만든다.
	// null이면 빈 Record({})로 기록되고, 불러오면 nullptr이 된다.
	static void SerializeMaterialReference(FStructuredArchive::FSlot Slot, UMaterial*& Material);
private:
	inline static uint16 NextSortID = 0;
};