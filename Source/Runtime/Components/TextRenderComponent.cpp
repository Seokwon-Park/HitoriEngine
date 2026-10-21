#include "EnginePCH.h"
#include "TextRenderComponent.h"

#include "Asset/AssetManager.h"
#include "Engine/World.h"

#include "Text/TextRenderer.h"

UTextRenderComponent::UTextRenderComponent()
{
	Font = UAssetManager::GetAssetByPath<UFont>("Fonts/CookieRun.json");
}

UTextRenderComponent::~UTextRenderComponent()
{
}

void UTextRenderComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UTextRenderComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

const FStaticMeshData* UTextRenderComponent::GetMeshData() const
{
	if (!Font) return nullptr;

	// 마지막으로 만든 뒤 바뀐 게 있으면 다시 만든다
	if (Text != CachedText || TextSize != CachedTextSize || Font != CachedFont)
	{
		CachedText = Text;
		CachedTextSize = TextSize;
		CachedFont = Font;
		bHasPickingMesh = RebuildPickingMesh();   // 1번 + 2번
	}
	return bHasPickingMesh ? &PickingMesh : nullptr;
}

bool UTextRenderComponent::RebuildPickingMesh() const
{
	float MinY, MaxY, MinZ, MaxZ;
	if (!FTextRenderer::ComputeTextBounds(CachedText, CachedTextSize, *CachedFont, MinY, MaxY, MinZ, MaxZ))
		return false;

	PickingMesh.Vertices.Reset();
	PickingMesh.Indices.Reset();

	FVertex Vertex;
	Vertex.Position = FVector(0, MinY, MinZ);
	PickingMesh.Vertices.Add(Vertex);
	Vertex.Position = FVector(0, MinY, MaxZ);
	PickingMesh.Vertices.Add(Vertex);
	Vertex.Position = FVector(0, MaxY, MaxZ);
	PickingMesh.Vertices.Add(Vertex);
	Vertex.Position = FVector(0, MaxY, MinZ);
	PickingMesh.Vertices.Add(Vertex);

	PickingMesh.Indices = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0,3,2 };

	PickingMesh.AABB.Min = FVector(-0.001f, MinY, MinZ);
	PickingMesh.AABB.Max = FVector(0.001f, MaxY, MaxZ);

	return true;
}
