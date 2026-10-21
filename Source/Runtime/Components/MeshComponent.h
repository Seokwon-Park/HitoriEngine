#pragma once

#include "PrimitiveComponent.h"

class UMeshComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
public:
	UMeshComponent();
	virtual ~UMeshComponent();

	virtual void BeginPlay();
	virtual void TickComponent(float DeltaTime);

	// 머티리얼 슬롯 API
	// 슬롯 개수와 이름, 덮어쓰기가 없을 때의 기본 머티리얼은 메시 쪽이 정하므로 파생 클래스가 답한다
	virtual int32 GetNumMaterials() const override { return 0; }
	virtual FString GetMaterialSlotName(int32 SlotIndex) const { return FString(); }
	virtual UMaterial* GetDefaultMaterial(int32 SlotIndex) const { return nullptr; }

	// 실제로 적용되는 머티리얼. 덮어쓰기가 있으면 그것, 없으면 메시 기본값
	UMaterial* GetMaterial(int32 SlotIndex) const override;
	// 이 컴포넌트만의 덮어쓰기. 없으면 nullptr
	UMaterial* GetOverrideMaterial(int32 SlotIndex) const;
	// 덮어쓰기 설정. nullptr을 넣으면 메시 기본값으로 되돌린다 (메시 에셋은 건드리지 않음)
	void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override;
	void ClearOverrideMaterials() { OverrideMaterials.Reset(); MarkRenderStateDirty(); }

	// 리플렉션 프로퍼티에 더해 슬롯별 덮어쓰기를 저장/로드한다
	virtual void Serialize(json& Handle, bool bIsLoading) override;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
protected:
	// 슬롯별 덮어쓰기. nullptr이면 그 슬롯은 메시 기본값을 쓴다
	TArray<UMaterial*> OverrideMaterials;
};
