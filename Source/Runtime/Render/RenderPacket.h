#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include <array>
#include <utility>
#include <algorithm>
#include <bit>

class FPrimitiveSceneProxy;
inline constexpr uint32 InvalidObjectSlot = ~0u;

struct FRenderPacket
{
	const FPrimitiveSceneProxy* Proxy = nullptr;
	const FMatrix* Model = nullptr;
	UStaticMesh* Mesh = nullptr;
	UMaterial* Material = nullptr;
	const void* MaterialParamData = nullptr;
	float CameraToParticleDistance = 0.0f;
	uint32 MaterialParamDataSize = 0;
	uint32 StartIndex = 0;
	uint32 IndexCount = 0; // 0이면 인덱스 버퍼 전체를 사용한다
	uint32 Slot = InvalidObjectSlot;
	uint8 LODIndex = 0;
	bool bOccludedByGpu = false;
};

struct FViewContext
{
	int32 ViewIndex = 0;
	uint32 Width = 0;
	uint32 Height = 0;
	FVector CameraPosition = FVector::ZeroVector;
	FVector CameraForward = FVector::ZeroVector;
	float ProjectionScaleSquared = 0.0f;
	float NearZ = 0.0f;
	bool bOrthographic = false;
	bool bGameView = false;
	float CameraDepth = 0.0f;
	FMatrix ViewProjection = FMatrix::Identity;
	void Prepare()
	{
		CameraDepth = CameraPosition.X * CameraForward.X
			+ CameraPosition.Y * CameraForward.Y + CameraPosition.Z * CameraForward.Z;
	}

	FMatrix BuildBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const
	{
		// 뷰포트 카메라를 바라보는 방향 벡터 계산
		FVector Facing = bOrthographic
			? (CameraForward * -1.0f)
			: (CameraPosition - WorldPosition).Normalized();
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		FVector Right = FVector::Cross(Up, Facing).Normalized();
		if (Right.Length() <= 1e-6f)
		{
			Up = FVector(0.0f, 1.0f, 0.0f);
			Right = FVector::Cross(Up, Facing).Normalized();
		}
		FVector RealUp = FVector::Cross(Facing, Right).Normalized();
		FMatrix Matrix = FMatrix::Identity;
		Matrix.M[0][0] = Facing.X;  Matrix.M[0][1] = Facing.Y;  Matrix.M[0][2] = Facing.Z;
		Matrix.M[1][0] = Right.X * Width;  Matrix.M[1][1] = Right.Y * Width;  Matrix.M[1][2] = Right.Z * Width;
		Matrix.M[2][0] = RealUp.X * Height; Matrix.M[2][1] = RealUp.Y * Height; Matrix.M[2][2] = RealUp.Z * Height;
		Matrix.M[3][0] = WorldPosition.X; Matrix.M[3][1] = WorldPosition.Y; Matrix.M[3][2] = WorldPosition.Z;
		return Matrix;
	}
};

// 바인딩을 공유하며 아이템마다 일반 인덱스 드로우를 한 번씩 실행한다
struct FStaticDrawItem
{
	const FPrimitiveSceneProxy* Proxy;
	uint32 Slot;
	uint32 StartIndex;
	uint32 IndexCount;
	uint32 bOccludedByGpu;
};

struct FStaticDrawGroup
{
	UMaterial* Material = nullptr;
	UStaticMesh* Mesh = nullptr;
	uint8 LODIndex = 0;
	std::vector<FStaticDrawItem> Items;
};

// 현재 프레임의 빌보드 및 파티클 행렬을 해당 패킷과 함께 보관한다
class FRenderQueue : public TArray<FRenderPacket>
{
	using FPackets = TArray<FRenderPacket>;
	static constexpr uint32 MatricesPerBlock = 256;
	using FMatrixBlock = std::array<FMatrix, MatricesPerBlock>;
	TArray<TUniquePtr<FMatrixBlock>> MatrixBlocks;
	uint32 MatrixCount = 0;
	uint32 FirstTranslucentIndex = 0;

	// 패킷 정렬 키 생성
	static uint64 MakeSortKey(const FRenderPacket& Packet)
	{
		if (Packet.Material->PSOType != EPSOType::StaticMesh_Opaque)
		{
			const uint64 DistanceBits = static_cast<uint64>(std::bit_cast<uint32>(Packet.CameraToParticleDistance));
			return (1ull << 63) | (~DistanceBits & 0xFFFFFFFFull);
		}

		return (static_cast<uint64>(Packet.Material->SortID) << 47)
			| (static_cast<uint64>(Packet.Mesh->SortID) << 31)
			| (static_cast<uint64>(Packet.LODIndex & 0x3) << 29);
	}

public:
	FRenderQueue() = default;
	FRenderQueue(const FRenderQueue&) = delete;
	FRenderQueue& operator=(const FRenderQueue&) = delete;
	FRenderQueue(FRenderQueue&& Other) noexcept
		: FPackets(std::move(Other)), MatrixBlocks(std::move(Other.MatrixBlocks)),
		  MatrixCount(std::exchange(Other.MatrixCount, 0)),
		  FirstTranslucentIndex(std::exchange(Other.FirstTranslucentIndex, 0)) {}
	FRenderQueue& operator=(FRenderQueue&& Other) noexcept
	{
		if (this != &Other)
		{
			FPackets::operator=(std::move(Other));
			MatrixBlocks = std::move(Other.MatrixBlocks);
			MatrixCount = std::exchange(Other.MatrixCount, 0);
			FirstTranslucentIndex = std::exchange(Other.FirstTranslucentIndex, 0);
		}
		return *this;
	}
	const FMatrix* StoreWorldMatrix(const FMatrix& World)
	{
		const uint32 BlockIndex = MatrixCount / MatricesPerBlock;
		if (BlockIndex >= static_cast<uint32>(MatrixBlocks.Num()))
			MatrixBlocks.Add(MakeUnique<FMatrixBlock>());
		FMatrix& Stored = (*MatrixBlocks[BlockIndex])[MatrixCount % MatricesPerBlock];
		Stored = World;
		++MatrixCount;
		return &Stored;
	}

	// 렌더 패킷 정렬
	void Sort()
	{
		if (IsEmpty())
		{
			FirstTranslucentIndex = 0;
			return;
		}

		struct FSortItem
		{
			uint64 Key = 0;
			int32 Index = 0;
		};

		const int32 TotalPackets = Num();
		TArray<FSortItem> SortItems;
		SortItems.Reserve(TotalPackets);
		for (int32 Index = 0; Index < TotalPackets; ++Index)
		{
			const FRenderPacket& Packet = (*this)[Index];
			if (!Packet.Mesh || !Packet.Material) continue;
			SortItems.Add({ MakeSortKey(Packet), Index });
		}

		std::sort(SortItems.begin(), SortItems.end(), [](const FSortItem& A, const FSortItem& B)
		{
			return A.Key < B.Key;
		});

		FirstTranslucentIndex = 0;
		while (FirstTranslucentIndex < static_cast<uint32>(SortItems.Num()) && !(SortItems[FirstTranslucentIndex].Key >> 63))
		{
			++FirstTranslucentIndex;
		}

		TArray<FRenderPacket> Sorted;
		Sorted.Reserve(Num());
		for (const FSortItem& Item : SortItems)
		{
			Sorted.Add(std::move((*this)[Item.Index]));
		}

		*static_cast<FPackets*>(this) = std::move(Sorted);
	}

	uint32 GetFirstTranslucentIndex() const { return FirstTranslucentIndex; }

	void Reset()
	{
		FPackets::Reset();
		MatrixCount = 0;
		FirstTranslucentIndex = 0;
	}
};
