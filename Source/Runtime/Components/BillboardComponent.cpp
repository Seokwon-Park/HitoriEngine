#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"
#include "Serialization/TypeSerializer.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

// Billboard 컴포넌트의 초기 상태를 구성한다.
UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
}

// Billboard 컴포넌트의 소멸을 처리한다.
UBillboardComponent::~UBillboardComponent()
{
}

// 기본 Mesh와 Material을 에셋 관리자에서 가져온다.
void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();

}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

bool UBillboardComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	if (!QuadMesh) return false;

	FMatrix BillboardMatrix;
	GetWorldTransformedMatrix(&BillboardMatrix);   // 카메라를 향하는, 실제로 그려지는 행 렬
	return TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardMatrix, OutHit);
}

// View별 렌더 행렬을 그대로 사용해 메인 카메라와 다른 방향에서도 같은 면을 선택한다.
bool UBillboardComponent::LineTraceComponentForView(
	const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix)
{
	return QuadMesh && TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardWorldMatrix, OutHit);
}

FBox UBillboardComponent::CalcLocalBounds() const
{
	// 카메라 방향에 따라 회전하므로 얇은 쿼드 AABB로는 컬링할 수 없다.
	const float Extent = bEditorIcon ? std::max(0.5f, IconSize) : 0.5f;
	return FBox{ FVector(-Extent, -Extent, -Extent), FVector(Extent, Extent, Extent) };
}

void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
	if (QuadMesh == nullptr || Material == nullptr)
	{
		return;
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();
	const float Width = bEditorIcon ? IconSize : WorldScale.Y;
	const float Height = bEditorIcon ? IconSize : WorldScale.Z;
	const FMatrix BillboardWorldMatrix = ViewContext.BuildBillboardMatrix(WorldPos, Width, Height);
	const uint32 PacketIndex = RenderQueue.Num();
	SubmitToRenderQueue(RenderQueue, BillboardWorldMatrix);
	if (static_cast<uint32>(RenderQueue.Num()) > PacketIndex)
	{
		const FVector Delta = WorldPos - ViewContext.CameraPosition;
		RenderQueue[PacketIndex].CameraToParticleDistance = Delta.Dot(Delta);
	}
}

// View별 Billboard 행렬과 Material을 렌더 패킷에 담는다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr || Material == nullptr)
		return;

	// Light Component light 가져와서 IconParams로 전달
	IconParams.Tint = TintSource ? TintSource->GetLightColor().ToVector4() : FVector4(1, 1, 1, 1);
	// 현재 반투명 패스는 톤 매핑 이후 SceneColor(LDR)에 그려진다.
	IconParams.DecodeSRGB = 0.0f;
	FRenderPacket Packet;
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.Model = RenderQueue.StoreWorldMatrix(BillboardWorldMatrix);
	Packet.MaterialParamData = &IconParams;
	Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
	RenderQueue.Add(Packet);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (Handle.contains("Material") && Handle["Material"].is_object())
		{
			if (UMaterial* Loaded = UMaterial::LoadMaterial(Handle["Material"]))
			{
				Material = Loaded;   // 못 만들었으면 생성자 기본값 유지
			}
		}
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}

// 카메라를 향하는 기저와 위치·크기로 Billboard 행렬을 구성한다.
void UBillboardComponent::GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const
{
	OutWorldMatrix->SetIdentity();

	const FTransform& Transform = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetTransform();

	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();

	FVector BbUp = Transform.GetUp().Normalized();
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);

	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	const FVector WorldPos = GetWorldLocation();

	// Icon 일 때만 IconSize 넘기기
	const FVector Scale = IsEditorIcon() ? FVector(IconSize, IconSize, IconSize) : GetWorldScale3D();
	
	// X -> Billboard Forward
	OutWorldMatrix->M[0][0] = BbFwd.X * Scale.X;
	OutWorldMatrix->M[0][1] = BbFwd.Y * Scale.X;
	OutWorldMatrix->M[0][2] = BbFwd.Z * Scale.X;
	OutWorldMatrix->M[0][3] = 0.0f;

	// Y -> Billboard Right
	OutWorldMatrix->M[1][0] = BbRight.X * Scale.Y;
	OutWorldMatrix->M[1][1] = BbRight.Y * Scale.Y;
	OutWorldMatrix->M[1][2] = BbRight.Z * Scale.Y;
	OutWorldMatrix->M[1][3] = 0.0f;

	// Z -> Billboard Up
	OutWorldMatrix->M[2][0] = BbUp.X * Scale.Z;
	OutWorldMatrix->M[2][1] = BbUp.Y * Scale.Z;
	OutWorldMatrix->M[2][2] = BbUp.Z * Scale.Z;
	OutWorldMatrix->M[2][3] = 0.0f;

	// Position
	OutWorldMatrix->M[3][0] = WorldPos.X;
	OutWorldMatrix->M[3][1] = WorldPos.Y;
	OutWorldMatrix->M[3][2] = WorldPos.Z;
	OutWorldMatrix->M[3][3] = 1.0f;
}

void UBillboardComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	UMaterial* Loaded = Material;
	UMaterial::SerializeMaterialReference(Record.EnterField("Material"), Loaded);

	// 불러오기에 실패하면(null) 생성자가 넣어 둔 기본 머티리얼을 유지한다
	if (Record.GetUnderlyingArchive().IsLoading() && Loaded)
		Material = Loaded;
}
