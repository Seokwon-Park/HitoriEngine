#pragma once

#include "Core/Types.h"
#include "Render/Buffer.h"
#include "Render/LightRenderData.h"
#include "Render/RenderEnums.h"

class FRenderer;
class FTexture2D;
struct FSceneView;

// 뷰별 셀 라이트 목록과 상수만 소유한다. GPU 명령은 FRenderer에 제출한다.
// Tiled는 슬라이스 1개짜리 Clustered로 돌리고, 2.5D는 같은 형식(슬라이스 1개)을 깊이 버퍼로 채우는 별도 CS다.
class FForwardLightTileGrid
{
public:
	bool Init();
	bool PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting,
		const FStructuredBuffer* SceneLights, const FTexture2D* Depth, ELightCullingMode InMode);
	bool ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights);
	void PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded);
	FConstantBuffer* GetPixelConstants() const { return GridCB.get(); }
	ID3D11Resource* GetWriteResource() const { return CellLightIndices.Buffer.Get(); }
	ID3D11ShaderResourceView* GetPixelResource() const { return bPixelTilesValid ? CellLightIndices.SRV.Get() : nullptr; }
	// 2.5D만 Depth Prepass 결과로 타일 깊이 범위를 잰다.
	bool RequiresDepth() const { return Mode == ELightCullingMode::Tiled25D; }
	void ResetPixelState() { bPixelTilesValid = false; }

private:
	// 컬링 방식별 셀 크기. TiledTileSize는 TiledLightCulling25D.hlsl의 TILE_DIM과 같아야 한다.
	static constexpr uint32 TiledTileSize = 16;
	static constexpr uint32 ClusteredTileSize = 32;
	static constexpr uint32 ClusteredSlices = 32;
	// LightCullingCommon.hlsli의 MAX_LIGHTS_PER_CELL / CELL_STRIDE와 같아야 한다.
	static constexpr uint32 MaxLightsPerCell = 32;
	static constexpr uint32 CellStride = MaxLightsPerCell + 1;

	struct alignas(16) FLightCullingConstants
	{
		FMatrix View;
		FMatrix Projection;
		FVector2 ViewportSize;
		uint32 NumTilesX = 0;
		uint32 NumTilesY = 0;
		uint32 NumLocalLights = 0;
		uint32 NumSlices = 1;
		uint32 TileSize = 0;
		uint32 bOrthographic = 0;
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		uint32 LocalLightOffset = 0;	// SceneLights에서 로컬 광원(포인트, 스포트, 렉트) 구간이 시작되는 위치
		uint32 Padding = 0;
	};
	static_assert(sizeof(FLightCullingConstants) == 176, "컬링 CS의 cbuffer FLightCullingConstants와 같아야 한다");

	struct alignas(16) FLightGridConstants
	{
		uint32 NumTilesX = 0;
		uint32 NumTilesY = 0;
		uint32 NumSlices = 1;
		uint32 GridMode = 0;		// ELightCullingMode. Off면 PS가 전체 로컬 광원을 순회한다
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float SliceScale = 0.0f;
		float SliceBias = 0.0f;
		uint32 TileSize = 0;
		uint32 bOrthographic = 0;
		float ProjZScale = 0.0f;	// Projection[0][2]. DeviceZ -> View 깊이 복원에 쓴다
		float ProjZOffset = 0.0f;	// Projection[3][2]
		uint32 NumLocalLights = 0;
		uint32 Padding[3]{};
	};
	static_assert(sizeof(FLightGridConstants) == 64, "LightGridCommon.hlsli의 cbuffer LightGridConstants와 같아야 한다");

	struct FCellBuffer
	{
		ComPtr<ID3D11Buffer> Buffer;
		ComPtr<ID3D11ShaderResourceView> SRV;
		ComPtr<ID3D11UnorderedAccessView> UAV;
		uint32 Capacity = 0;
		bool EnsureCapacity(uint32 Count, uint32 Stride);
	};

	ComPtr<ID3D11ComputeShader> CellCullingShader;	// TileBasedLightCulling.hlsl (Tiled, Clustered)
	ComPtr<ID3D11ComputeShader> Tiled25DShader;		// TiledLightCulling25D.hlsl
	TUniquePtr<FConstantBuffer> CullingCB;
	TUniquePtr<FConstantBuffer> GridCB;
	// 셀마다 [개수][라이트 번호 x MaxLightsPerCell] 형태로 CellStride칸씩 이어져 있다.
	FCellBuffer CellLightIndices;
	FLightCullingConstants CullingConstants{};
	FLightGridConstants GridConstants{};
	ELightCullingMode Mode = ELightCullingMode::Off;
	bool bPixelTilesValid = false;
};
