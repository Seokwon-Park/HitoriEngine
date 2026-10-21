#pragma once

#include "PrimitiveComponent.h"
#include "LightComponentBase.h"

struct FSubUVConstants
{
	// 한 파티클을 그릴 때 Shader에 전달할 Atlas 프레임과 투명도 값을 담는다.
	float CurrentFrame;
	float AtlasRowSize;
	float AtlasColSize;
	//float Padding = 0.0f;
	float Alpha;
	FVector4 Tint = FVector4(1, 1, 1, 1);	// default 흰색
	// 1이면 sRGB -> 선형 변환한다. 현재 반투명 패스는 LDR이므로 0을 전달한다.
	float DecodeSRGB = 1.0f;
	float Padding[3]{};
};
static_assert(sizeof(FSubUVConstants) == 48, "ParticleSubUVShader.hlsl cbuffer subuv와 크기가 같아야 한다");

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit) override;
	// 클릭한 View의 실제 렌더 행렬로 Quad Mesh 교차를 판정한다.
	bool LineTraceComponentForView(const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix);

	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override { return SlotIndex == 0 ? Material : nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override { if (SlotIndex == 0) Material = InMaterial; }

	void SetTintSource(const ULightComponentBase* InSource) { TintSource = InSource; }

	virtual const FStaticMeshData* GetMeshData() const override { return QuadMesh ? &QuadMesh->GetMeshData() : nullptr; }
	virtual FBox CalcLocalBounds() const override;

	void GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const;
	// 뷰 컨텍스트 기준 월드 행렬로 Billboard 렌더 패킷을 제출한다.
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext) override;
	// View별 ViewportLayout가 계산한 Billboard 행렬을 사용해 같은 렌더 패킷 형식으로 제출한다.
	void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix);

	virtual void Serialize(json& Handle, bool bIsLoading) override;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	bool IsEditorIcon() const { return bEditorIcon; }
	void SetEditorIcon(bool IsEditorIcon) { bEditorIcon = IsEditorIcon; }
	void SetIconSize(float S) { IconSize = S; }

protected:
	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;
	FSubUVConstants IconParams{ 0.0f, 1.0f, 1.0f, 1.0f };	// CurrentFrame, RowSize, ColSize, Alpha
private:
	const ULightComponentBase* TintSource = nullptr;
	bool bEditorIcon = false;
	float IconSize = 1.0f;

};
