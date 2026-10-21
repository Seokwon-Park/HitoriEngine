#include "EnginePCH.h"
#include "GPUOcclusion.h"

#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Render/Shader.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Core/Async/TaskPool.h"
#include "Core/Stats/LightweightStats.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstring>

DECLARE_CYCLE_STAT("GPU Occlusion (Total)", STAT_GPUOcclusion);
DECLARE_CYCLE_STAT("  Occ Prepare+Occluders", STAT_GPUOcclusionPrepare);
DECLARE_CYCLE_STAT("  Occ Draw", STAT_GPUOcclusionDraw);
DECLARE_CYCLE_STAT("  Occ Wait GPU", STAT_GPUOcclusionWait);

namespace
{
	// 셰이더 폴더의 .hlsl은 전부 VS/PS로 자동 컴파일되므로 컴퓨트 셰이더는 소스를 여기에 둔다.
	constexpr char OcclusionShaderSource[] = R"(
#pragma pack_matrix(row_major)
// ---------- Hi-Z 만들기: 밉 L 텍셀 = 밉 L-1의 해당 영역 중 가장 먼 깊이 ----------
cbuffer HiZParams : register(b0)
{
	uint2 SrcSize;
	uint2 DstSize;
};
Texture2D<float> HiZSrc : register(t0);
RWTexture2D<float> HiZDst : register(u0);

[numthreads(8, 8, 1)]
void BuildHiZ(uint3 Id : SV_DispatchThreadID)
{
	if (Id.x >= DstSize.x || Id.y >= DstSize.y)
		return;

	// 보통은 2x2. 원본이 홀수 크기면 마지막 줄·칸 텍셀이 남는 한 줄까지 맡는다 (빠뜨리면 잘못 가림).
	const uint2 First = Id.xy * 2;
	uint2 Last = min(First + 1, SrcSize - 1);
	if (Id.x == DstSize.x - 1) Last.x = SrcSize.x - 1;
	if (Id.y == DstSize.y - 1) Last.y = SrcSize.y - 1; 

	float MaxDepth = 0.0f;
	for (uint Y = First.y; Y <= Last.y; ++Y)
		for (uint X = First.x; X <= Last.x; ++X)
			MaxDepth = max(MaxDepth, HiZSrc.Load(int3(X, Y, 0)));
	HiZDst[Id.xy] = MaxDepth;
}

// ---------- 물체별 AABB 판정 ----------
cbuffer CullParams : register(b1)
{
	matrix ViewProjection;     // CPU row-major 데이터를 그대로 사용한다
	float2 ScreenSize;
	uint NumItems;
	uint Padding;
};

struct FCullItem
{
	float3 Center;
	float Padding0;
	float3 Extent;
	float Padding1;
};
StructuredBuffer<FCullItem> Items : register(t1);
Texture2D<float> HiZ : register(t2);
RWBuffer<uint> Result : register(u1);

bool IsOccluded(FCullItem Item)
{
	if (Item.Extent.x < 0.0f)
		return false;                       // 판정하지 않는 물체

	float2 NdcMin = 1e30f, NdcMax = -1e30f;
	float MinZ = 1.0f;
	[unroll]
	for (uint c = 0; c < 8; ++c)
	{
		const float3 Sign = float3((c & 1) ? 1.0f : -1.0f, (c & 2) ? 1.0f : -1.0f, (c & 4) ? 1.0f : -1.0f);
		const float4 Clip = mul(float4(Item.Center + Item.Extent * Sign, 1.0f), ViewProjection);
		if (Clip.w <= 1e-5f || Clip.z < 0.0f)
			return false;                   // 근평면에 걸침 -> 보이는 것으로
		const float3 Ndc = Clip.xyz / Clip.w;
		NdcMin = min(NdcMin, Ndc.xy);
		NdcMax = max(NdcMax, Ndc.xy);
		MinZ = min(MinZ, Ndc.z);
	}

	// NDC -> 화면 UV (y는 아래로)
	float2 UvMin = float2(NdcMin.x * 0.5f + 0.5f, 0.5f - NdcMax.y * 0.5f);
	float2 UvMax = float2(NdcMax.x * 0.5f + 0.5f, 0.5f - NdcMin.y * 0.5f);
	if (UvMax.x <= 0.0f || UvMax.y <= 0.0f || UvMin.x >= 1.0f || UvMin.y >= 1.0f)
		return false;                       // 화면 밖은 프러스텀 컬링 담당
	UvMin = saturate(UvMin);
	UvMax = saturate(UvMax);

	// Hi-Z 밉0 텍셀 = 화면 2x2 픽셀. 사각형이 2x2 텍셀 안에 들어가는 밉을 고른다.
	const float2 TexelMin = UvMin * ScreenSize * 0.5f;
	const float2 TexelMax = UvMax * ScreenSize * 0.5f;
	const float Size = max(TexelMax.x - TexelMin.x, TexelMax.y - TexelMin.y);

	uint Width, Height, Levels;
	HiZ.GetDimensions(0, Width, Height, Levels);
	uint Level = Size <= 1.0f ? 0 : (uint)ceil(log2(Size));
	Level = min(Level, Levels - 1);
	HiZ.GetDimensions(Level, Width, Height, Levels);

	const int2 MaxTexel = int2(Width - 1, Height - 1);
	const int2 T0 = min(int2(TexelMin) >> Level, MaxTexel);
	const int2 T1 = min(int2(TexelMax) >> Level, MaxTexel);

	float MaxDepth = 0.0f;
	for (int Y = T0.y; Y <= T1.y; ++Y)
		for (int X = T0.x; X <= T1.x; ++X)
			MaxDepth = max(MaxDepth, HiZ.Load(int3(X, Y, Level)));

	// 물체의 가장 가까운 점조차 그 영역의 가장 먼 가림막보다 뒤 -> 완전히 가려짐
	return MinZ > MaxDepth;
}

[numthreads(64, 1, 1)]
void Cull(uint3 Id : SV_DispatchThreadID)
{
	if (Id.x >= NumItems)
		return;
	Result[Id.x] = IsOccluded(Items[Id.x]) ? 1 : 0;
}
)";

	ComPtr<ID3D11ComputeShader> CompileComputeShader(ID3D11Device* Device, const char* EntryPoint)
	{
		const UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
		ComPtr<ID3DBlob> Code, Errors;
		const HRESULT Hr = D3DCompile(OcclusionShaderSource, sizeof(OcclusionShaderSource) - 1, "GPUOcclusion",
			nullptr, nullptr, EntryPoint, "cs_5_0", Flags, 0, Code.GetAddressOf(), Errors.GetAddressOf());
		if (FAILED(Hr))
		{
			if (Errors)
				OutputDebugStringA(static_cast<const char*>(Errors->GetBufferPointer()));
			HTR_LOG(Error, "[GPUOcclusion] compute shader {} compile failed", EntryPoint);
			return nullptr;
		}

		ComPtr<ID3D11ComputeShader> Shader;
		if (FAILED(Device->CreateComputeShader(Code->GetBufferPointer(), Code->GetBufferSize(), nullptr, Shader.GetAddressOf())))
			return nullptr;
		return Shader;
	}

	ComPtr<ID3D11Buffer> CreateDefaultConstantBuffer(ID3D11Device* Device, uint32 Size)
	{
		D3D11_BUFFER_DESC Desc{};
		Desc.ByteWidth = (Size + 15) & ~15u;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ComPtr<ID3D11Buffer> Buffer;
		Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf());
		return Buffer;
	}

	struct FHiZParams
	{
		uint32 SrcSize[2];
		uint32 DstSize[2];
	};

	struct FCullParams
	{
		FMatrix ViewProjection;
		float ScreenSize[2];
		uint32 NumItems;
		uint32 Padding;
	};

	// 선행 패스의 드로우 하나. 그리는 루프가 프록시를 다시 따라가지 않도록 필요한 값만 담는다.
	struct FOccluderDraw
	{
		uint32 Slot;            // 가림막 상수 버퍼 칸
		uint32 StartIndex;
		uint32 IndexCount;
	};

	// 메시·LOD가 같은 드로우 묶음. 모든 가림막은 같은 깊이 전용 셰이더를 쓴다.
	struct FOccluderBucket
	{
		UStaticMesh* Mesh;
		uint32 LOD;
		std::vector<FOccluderDraw> Draws;
	};
	std::vector<FOccluderBucket> OccluderBuckets;   // 용량 재사용 (렌더 스레드 하나만 쓴다)
}

bool FGPUOcclusion::Init()
{
	ID3D11Device* Device = RenderCommand::GetDevice();

	BuildHiZCS = CompileComputeShader(Device, "BuildHiZ");
	CullCS = CompileComputeShader(Device, "Cull");
	HiZParamsCB = CreateDefaultConstantBuffer(Device, sizeof(FHiZParams));
	CullParamsCB = CreateDefaultConstantBuffer(Device, sizeof(FCullParams));
	ViewCB = CreateDefaultConstantBuffer(Device, sizeof(FMatrix));

	// 가림막은 물체마다 256바이트 칸을 오프셋으로 바인딩해 그린다 (D3D11.1).
	const bool bReady = BuildHiZCS && CullCS && HiZParamsCB && CullParamsCB && ViewCB &&
		RenderCommand::SupportsConstantBufferOffsets();
	if (!bReady)
		HTR_LOG(Error, "[GPUOcclusion] init failed. GPU occlusion culling is unavailable.");
	return bReady;
}

bool FGPUOcclusion::EnsureTargets(uint32 Width, uint32 Height)
{
	if (OccluderDSV && TargetWidth == Width && TargetHeight == Height)
		return true;

	ID3D11Device* Device = RenderCommand::GetDevice();
	TargetWidth = TargetHeight = 0;
	OccluderDepth.Reset(); OccluderDSV.Reset(); OccluderDepthSRV.Reset();
	HiZ.Reset(); HiZSRV.Reset(); HiZMipSRVs.clear(); HiZMipUAVs.clear();
	MipWidths.clear(); MipHeights.clear();

	// 가림막 깊이: DSV로 쓰고 SRV로 읽어야 해서 TYPELESS로 만든다.
	D3D11_TEXTURE2D_DESC DepthDesc{};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, OccluderDepth.GetAddressOf())))
		return false;

	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
	DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC DepthSrvDesc{};
	DepthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DepthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	DepthSrvDesc.Texture2D.MipLevels = 1;
	if (FAILED(Device->CreateDepthStencilView(OccluderDepth.Get(), &DsvDesc, OccluderDSV.GetAddressOf())) ||
		FAILED(Device->CreateShaderResourceView(OccluderDepth.Get(), &DepthSrvDesc, OccluderDepthSRV.GetAddressOf())))
		return false;

	// Hi-Z: 화면 절반(올림)에서 1x1까지. D3D 밉 크기는 내림으로 줄어드는데, 홀수로 남는 줄은 BuildHiZ가 마지막 텍셀에 합친다.
	const uint32 Width0 = std::max(1u, (Width + 1) / 2);
	const uint32 Height0 = std::max(1u, (Height + 1) / 2);
	uint32 MipCount = 1;
	for (uint32 Size = std::max(Width0, Height0); Size > 1; Size >>= 1)
		++MipCount;

	D3D11_TEXTURE2D_DESC HiZDesc{};
	HiZDesc.Width = Width0;
	HiZDesc.Height = Height0;
	HiZDesc.MipLevels = MipCount;
	HiZDesc.ArraySize = 1;
	HiZDesc.Format = DXGI_FORMAT_R32_FLOAT;
	HiZDesc.SampleDesc.Count = 1;
	HiZDesc.Usage = D3D11_USAGE_DEFAULT;
	HiZDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	if (FAILED(Device->CreateTexture2D(&HiZDesc, nullptr, HiZ.GetAddressOf())) ||
		FAILED(Device->CreateShaderResourceView(HiZ.Get(), nullptr, HiZSRV.GetAddressOf())))
		return false;

	HiZMipSRVs.resize(MipCount);
	HiZMipUAVs.resize(MipCount);
	for (uint32 Level = 0; Level < MipCount; ++Level)
	{
		MipWidths.push_back(std::max(1u, Width0 >> Level));
		MipHeights.push_back(std::max(1u, Height0 >> Level));

		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Texture2D.MostDetailedMip = Level;
		SrvDesc.Texture2D.MipLevels = 1;
		D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc{};
		UavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		UavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		UavDesc.Texture2D.MipSlice = Level;
		if (FAILED(Device->CreateShaderResourceView(HiZ.Get(), &SrvDesc, HiZMipSRVs[Level].GetAddressOf())) ||
			FAILED(Device->CreateUnorderedAccessView(HiZ.Get(), &UavDesc, HiZMipUAVs[Level].GetAddressOf())))
			return false;
	}

	TargetWidth = Width;
	TargetHeight = Height;
	return true;
}

bool FGPUOcclusion::EnsureItemCapacity(uint32 Count)
{
	if (Count <= ItemCapacity && ItemBuffer && ResultBuffer)
		return true;

	uint32 NewCapacity = std::max(ItemCapacity * 2, 4096u);
	while (NewCapacity < Count)
		NewCapacity *= 2;

	ID3D11Device* Device = RenderCommand::GetDevice();
	ItemBuffer.Reset(); ItemSRV.Reset(); ResultBuffer.Reset(); ResultUAV.Reset(); ResultStaging.Reset();
	ItemCapacity = 0;

	D3D11_BUFFER_DESC ItemDesc{};
	ItemDesc.ByteWidth = NewCapacity * sizeof(FCullItem);
	ItemDesc.Usage = D3D11_USAGE_DYNAMIC;
	ItemDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	ItemDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ItemDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	ItemDesc.StructureByteStride = sizeof(FCullItem);
	D3D11_SHADER_RESOURCE_VIEW_DESC ItemSrvDesc{};
	ItemSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	ItemSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	ItemSrvDesc.Buffer.NumElements = NewCapacity;
	if (FAILED(Device->CreateBuffer(&ItemDesc, nullptr, ItemBuffer.GetAddressOf())) ||
		FAILED(Device->CreateShaderResourceView(ItemBuffer.Get(), &ItemSrvDesc, ItemSRV.GetAddressOf())))
		return false;

	D3D11_BUFFER_DESC ResultDesc{};
	ResultDesc.ByteWidth = NewCapacity * sizeof(uint32);
	ResultDesc.Usage = D3D11_USAGE_DEFAULT;
	ResultDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	D3D11_UNORDERED_ACCESS_VIEW_DESC ResultUavDesc{};
	ResultUavDesc.Format = DXGI_FORMAT_R32_UINT;
	ResultUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	ResultUavDesc.Buffer.NumElements = NewCapacity;
	if (FAILED(Device->CreateBuffer(&ResultDesc, nullptr, ResultBuffer.GetAddressOf())) ||
		FAILED(Device->CreateUnorderedAccessView(ResultBuffer.Get(), &ResultUavDesc, ResultUAV.GetAddressOf())))
		return false;

	D3D11_BUFFER_DESC StagingDesc{};
	StagingDesc.ByteWidth = NewCapacity * sizeof(uint32);
	StagingDesc.Usage = D3D11_USAGE_STAGING;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	if (FAILED(Device->CreateBuffer(&StagingDesc, nullptr, ResultStaging.GetAddressOf())))
		return false;

	ItemCapacity = NewCapacity;
	return true;
}

bool FGPUOcclusion::EnsureOccluderCapacity(uint32 Count)
{
	if (Count <= OccluderCapacity && OccluderCB)
		return true;

	uint32 NewCapacity = std::max(OccluderCapacity * 2, 1024u);
	while (NewCapacity < Count)
		NewCapacity *= 2;
	OccluderCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	OccluderCapacity = (OccluderCB && OccluderCB->GetBuffer()) ? NewCapacity : 0;
	return OccluderCapacity != 0;
}

namespace
{
	// 점수(반지름²/거리²)를 1/4 옥타브 단위 칸으로 나눈다. 0은 "가림막 후보 아님"으로 쓴다.
	constexpr uint32 ScoreBucketCount = 256;
	constexpr int32 ScoreBucketOffset = 160;   // 점수 1e-8 ~ 1e4 정도가 1 ~ 255 안에 들어온다

	uint8 ToScoreBucket(float Score)
	{
		const int32 Bucket = static_cast<int32>(std::floor(std::log2(Score) * 4.0f)) + ScoreBucketOffset;
		return static_cast<uint8>(std::clamp(Bucket, 1, static_cast<int32>(ScoreBucketCount) - 1));
	}
}

// 물체마다 판정 입력(AABB)을 채우고, 가림막 후보 점수(화면 크기)를 매긴다.
// 점수 칸별로 개수와 화면 넓이 합을 조각마다 따로 모아 두면, 가림막 커트라인을 정렬 없이 잡을 수 있다.
bool FGPUOcclusion::FillItems(const FPrimitiveSceneProxy* const* Proxies, uint32 Count, const FViewContext& View)
{
	ScoreBuckets.resize(Count);
	OccluderLODs.resize(Count);

	// 점수 -> 화면에서 차지하는 비율. 타원 넓이 π·rx·ry를 NDC 화면 넓이 4로 나눈 것 (r² ≈ Extent²/3, 구에 가까운 물체 기준).
	// ScaleX·ScaleY = (둘 중 큰 쪽)² × 짧은 변/긴 변
	const float Aspect = static_cast<float>(std::min(View.Width, View.Height)) / static_cast<float>(std::max(View.Width, View.Height));
	CoverageScale = 3.14159265f / 12.0f * View.ProjectionScaleSquared * Aspect;

	FTaskPool& Pool = FTaskPool::Get();
	const uint32 ChunkCount = std::clamp(Count / 1024u, 1u, Pool.GetNumThreads() * 4);
	HistogramCounts.assign(size_t(ChunkCount) * ScoreBucketCount, 0u);
	HistogramScores.assign(size_t(ChunkCount) * ScoreBucketCount, 0.0f);
	HistogramChunks = ChunkCount;   // 아래에서 실패해도 0으로 채워진 히스토그램을 읽게 된다 (가림막 없음)

	D3D11_MAPPED_SUBRESOURCE Mapped{};
	if (FAILED(RenderCommand::GetContext()->Map(ItemBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		std::fill(ScoreBuckets.begin(), ScoreBuckets.end(), uint8(0));
		return false;   // 판정 입력을 못 채웠으면 이번 프레임은 오클루전 없이 그린다
	}
	FCullItem* Dest = static_cast<FCullItem*>(Mapped.pData);

	Pool.ParallelFor(Count, ChunkCount, [&](uint32 Begin, uint32 End, uint32 ChunkIndex)
		{
			uint32* Counts = HistogramCounts.data() + size_t(ChunkIndex) * ScoreBucketCount;   // 조각 전용
			float* ScoreSums = HistogramScores.data() + size_t(ChunkIndex) * ScoreBucketCount;

			for (uint32 i = Begin; i < End; ++i)
			{
				const FPrimitiveSceneProxy* Proxy = Proxies[i];
				const FAABB& Bounds = Proxy->GetBounds();

				FCullItem Item{};
				Item.Center = Bounds.Center;
				Item.Extent = Bounds.Extent;
				std::memcpy(Dest + i, &Item, sizeof(FCullItem));   // 쓰기 전용 메모리라 한 번에 쓴다

				// 캐시된 메시가 있는 물체만 가림막이 될 수 있다. 화면에 크게 보일수록(반지름²/거리²) 많이 가린다.
				uint8 Bucket = 0;
				if (Proxy->IsVisible() && Proxy->GetMesh())
				{
					const FVector ToCenter = Bounds.Center - View.CameraPosition;
					const float Score = Bounds.Extent.Dot(Bounds.Extent) / std::max(ToCenter.Dot(ToCenter), 1e-4f);
					Bucket = ToScoreBucket(Score);
					++Counts[Bucket];
					ScoreSums[Bucket] += Score;
					OccluderLODs[i] = static_cast<uint8>(SelectLOD(*Proxy, View));   // 가림막이 되면 본 패스와 같은 LOD로 그린다
				}
				ScoreBuckets[i] = Bucket;
			}
		});

	RenderCommand::GetContext()->Unmap(ItemBuffer.Get(), 0);
	return true;
}

// 화면에 크게 보이는 물체를 실제 메시로 가림막 깊이 버퍼에 그린다. 본 패스와 같은 셰이더·행렬·LOD라 깊이가 정확하다.
void FGPUOcclusion::DrawOccluders(const FPrimitiveSceneProxy* const* Proxies, const FViewContext& View)
{
	// 가장 먼저 비운다. 가림막을 하나도 못 그리고 끝나도 깊이가 1(아무것도 안 가림)이어야 한다.
	// (지난 프레임 깊이가 남아 있으면 지금 보이는 물체를 가려진 것으로 판정할 수 있다.)
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	Context->ClearDepthStencilView(OccluderDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	{
	// 가림막 고르기: 화면에 크게 보이는 것부터 넓이를 쌓아, 화면 OccluderCoverage장분을 덮거나 MaxOccluders개가 되면 멈춘다.
	//  - 가까이 가면 사과 하나하나가 커서 몇백 개면 화면이 덮인다 -> 가림막이 적어져 선행 패스가 싸다.
	//  - 멀리서 보면 사과가 작아 많이 뽑힌다 (여러 겹이 함께 가리므로 많이 필요).
	// 점수 칸 히스토그램을 위에서부터 누적해 커트라인 칸(Cut)을 잡는다. 정렬이 필요 없다.
	const uint32 Count = static_cast<uint32>(ScoreBuckets.size());
	uint32 Counts[ScoreBucketCount] = {};
	double ScoreSums[ScoreBucketCount] = {};
	for (uint32 c = 0; c < HistogramChunks; ++c)
		for (uint32 b = 0; b < ScoreBucketCount; ++b)
		{
			Counts[b] += HistogramCounts[size_t(c) * ScoreBucketCount + b];
			ScoreSums[b] += HistogramScores[size_t(c) * ScoreBucketCount + b];
		}

	const uint32 Budget = std::min(Settings.MaxOccluders, Count);
	uint32 Cut = ScoreBucketCount;      // 이 칸 이상이 가림막 (ScoreBucketCount면 하나도 없음)
	uint32 Accumulated = 0;
	double Coverage = 0.0;
	for (int32 b = ScoreBucketCount - 1; b >= 1 && Budget > 0; --b)
	{
		if (Counts[b] == 0)
			continue;
		Cut = static_cast<uint32>(b);
		Accumulated += Counts[b];
		Coverage += ScoreSums[b] * CoverageScale;
		if (Accumulated >= Budget || Coverage >= Settings.OccluderCoverage)
			break;
	}
	Stats.Coverage = static_cast<float>(Coverage);

	for (FOccluderBucket& Bucket : OccluderBuckets)
		Bucket.Draws.clear();
	Stats.Occluders = 0;
	Stats.OccluderTriangles = 0;

	if (Budget == 0 || Cut >= ScoreBucketCount || !EnsureOccluderCapacity(Budget))
		return;

	// 고르면서 바로 행렬을 칸에 쓰고 드로우 정보를 묶음에 넣는다. 프록시를 읽는 것은 여기 한 번뿐이다.
	uint8* Slots = static_cast<uint8*>(RenderCommand::MapWriteDiscard(OccluderCB.get()));
	if (!Slots)
		return;

	// 커트라인 칸은 개수 제한에 걸려 일부만 들어갈 수 있으므로, 그보다 위 칸을 먼저 모두 담고 커트라인 칸은 나중에 담는다.
	uint32 Selected = 0;
	for (uint32 Pass = 0; Pass < 2; ++Pass)
	for (uint32 i = 0; i < Count && Selected < Budget; ++i)
	{
		const uint32 ScoreBucket = ScoreBuckets[i];
		if (Pass == 0 ? ScoreBucket <= Cut : ScoreBucket != Cut)
			continue;

		const FPrimitiveSceneProxy* Proxy = Proxies[i];
		const uint32 LOD = OccluderLODs[i];                 // FillItems가 병렬로 골라 둔 LOD (본 패스와 같음)
		const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);

		bool bAnyOpaque = false;
		for (uint32 s = 0; s < CachedLOD.NumSections; ++s)
		{
			const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + s);
			// 반투명 부분은 뒤를 가리지 않는다.
			if (!Section.Material || Section.Material->BlendState != EBlendState::Opaque)
				continue;

			FOccluderBucket* Target = nullptr;
			for (FOccluderBucket& Bucket : OccluderBuckets)
				if (Bucket.Mesh == Proxy->GetMesh() && Bucket.LOD == LOD)
				{
					Target = &Bucket;
					break;
				}
			if (!Target)
				Target = &OccluderBuckets.emplace_back(FOccluderBucket{ Proxy->GetMesh(), LOD, {} });
			Target->Draws.push_back({ Selected, Section.StartIndex, Section.IndexCount });
			Stats.OccluderTriangles += Section.IndexCount / 3;
			bAnyOpaque = true;
		}

		if (bAnyOpaque)
		{
			std::memcpy(Slots + size_t(Selected) * ObjectSlotBytes, &Proxy->GetLocalToWorld(), sizeof(FMatrix));
			++Selected;
		}
	}
	RenderCommand::Unmap(OccluderCB.get());
	Stats.Occluders = Selected;
	}

	SCOPE_CYCLE_COUNTER(STAT_GPUOcclusionDraw);

	Context->UpdateSubresource(ViewCB.Get(), 0, nullptr, &View.ViewProjection, 0, 0);

	Context->OMSetRenderTargets(0, nullptr, OccluderDSV.Get());   // 색 없이 깊이만
	const D3D11_VIEWPORT Viewport{ 0.0f, 0.0f, static_cast<float>(TargetWidth), static_cast<float>(TargetHeight), 0.0f, 1.0f };
	Context->RSSetViewports(1, &Viewport);

	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->VSSetConstantBuffers(0, 1, ViewCB.GetAddressOf());
	FPipelineState* DepthPipeline = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_DepthOnly);
	if (!DepthPipeline || !DepthPipeline->Shader || !DepthPipeline->Shader->VertexShader)
	{
		Context->OMSetRenderTargets(0, nullptr, nullptr);
		return;
	}
	RenderCommand::BindShaderProgram(DepthPipeline->Shader);
	Context->PSSetShader(nullptr, nullptr, 0);

	constexpr uint32 SlotConstants = ObjectSlotBytes / 16;
	for (const FOccluderBucket& Bucket : OccluderBuckets)
	{
		if (Bucket.Draws.empty())
			continue;
		RenderCommand::BindMesh(Bucket.Mesh, Bucket.LOD);

		for (const FOccluderDraw& Draw : Bucket.Draws)
		{
			RenderCommand::BindConstantBufferRange(2, OccluderCB.get(), Draw.Slot * SlotConstants, SlotConstants, EShaderBindFlagBits::Vertex);
			RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
		}
	}

	Context->OMSetRenderTargets(0, nullptr, nullptr);   // 깊이를 SRV로 읽기 전에 풀어 둔다
}

void FGPUOcclusion::BuildHiZ()
{
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	Context->CSSetShader(BuildHiZCS.Get(), nullptr, 0);
	Context->CSSetConstantBuffers(0, 1, HiZParamsCB.GetAddressOf());

	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	for (uint32 Level = 0; Level < static_cast<uint32>(HiZMipUAVs.size()); ++Level)
	{
		FHiZParams Params;
		Params.SrcSize[0] = Level == 0 ? TargetWidth : MipWidths[Level - 1];
		Params.SrcSize[1] = Level == 0 ? TargetHeight : MipHeights[Level - 1];
		Params.DstSize[0] = MipWidths[Level];
		Params.DstSize[1] = MipHeights[Level];
		Context->UpdateSubresource(HiZParamsCB.Get(), 0, nullptr, &Params, 0, 0);

		ID3D11ShaderResourceView* Src = Level == 0 ? OccluderDepthSRV.Get() : HiZMipSRVs[Level - 1].Get();
		Context->CSSetShaderResources(0, 1, &Src);
		Context->CSSetUnorderedAccessViews(0, 1, HiZMipUAVs[Level].GetAddressOf(), nullptr);
		Context->Dispatch((MipWidths[Level] + 7) / 8, (MipHeights[Level] + 7) / 8, 1);

		// 다음 밉에서 이 밉을 읽으므로 쓰기 바인딩을 먼저 푼다.
		Context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
		Context->CSSetShaderResources(0, 1, &NullSRV);
	}
}

void FGPUOcclusion::Cull(const FMatrix& ViewProjection, uint32 Count)
{
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	FCullParams Params;
	Params.ViewProjection = ViewProjection;
	Params.ScreenSize[0] = static_cast<float>(TargetWidth);
	Params.ScreenSize[1] = static_cast<float>(TargetHeight);
	Params.NumItems = Count;
	Params.Padding = 0;
	Context->UpdateSubresource(CullParamsCB.Get(), 0, nullptr, &Params, 0, 0);

	Context->CSSetShader(CullCS.Get(), nullptr, 0);
	Context->CSSetConstantBuffers(1, 1, CullParamsCB.GetAddressOf());
	ID3D11ShaderResourceView* SRVs[2] = { ItemSRV.Get(), HiZSRV.Get() };
	Context->CSSetShaderResources(1, 2, SRVs);
	Context->CSSetUnorderedAccessViews(1, 1, ResultUAV.GetAddressOf(), nullptr);

	Context->Dispatch((Count + 63) / 64, 1, 1);

	ID3D11ShaderResourceView* NullSRVs[3] = {};
	ID3D11UnorderedAccessView* NullUAVs[2] = {};
	Context->CSSetShaderResources(0, 3, NullSRVs);
	Context->CSSetUnorderedAccessViews(0, 2, NullUAVs, nullptr);
	Context->CSSetShader(nullptr, nullptr, 0);

	Context->CopyResource(ResultStaging.Get(), ResultBuffer.Get());
}

// 판정 결과를 기다려 읽는다. 여기서 CPU가 GPU를 기다린다 (GPU에 쌓인 이전 작업 + 가림막 패스 + 판정).
bool FGPUOcclusion::ReadBack(uint32 Count)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUOcclusionWait);
	const auto Start = std::chrono::high_resolution_clock::now();

	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	D3D11_MAPPED_SUBRESOURCE Mapped{};
	if (FAILED(Context->Map(ResultStaging.Get(), 0, D3D11_MAP_READ, 0, &Mapped)))
		return false;

	const uint32* Result = static_cast<const uint32*>(Mapped.pData);
	Occluded.resize(Count);
	uint32 OccludedCount = 0;
	for (uint32 i = 0; i < Count; ++i)
	{
		Occluded[i] = static_cast<uint8>(Result[i]);
		OccludedCount += Result[i];
	}
	Context->Unmap(ResultStaging.Get(), 0);

	Stats.Occluded = OccludedCount;
	Stats.WaitMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - Start).count();
	return true;
}

bool FGPUOcclusion::Run(const FPrimitiveSceneProxy* const* Proxies, uint32 Count, const FViewContext& View)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUOcclusion);

	Stats.Tested = Count;
	Stats.Occluded = 0;
	if (!Settings.bEnabled || Count == 0 || View.Width == 0 || View.Height == 0 || !CullCS || !BuildHiZCS)
		return false;
	if (!EnsureTargets(View.Width, View.Height) || !EnsureItemCapacity(Count))
		return false;

	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 호출한 쪽의 렌더 타깃·뷰포트를 기억했다가 되돌린다 (Get은 참조를 올리므로 ComPtr로 받는다).
	ComPtr<ID3D11RenderTargetView> SavedRTV;
	ComPtr<ID3D11DepthStencilView> SavedDSV;
	Context->OMGetRenderTargets(1, SavedRTV.GetAddressOf(), SavedDSV.GetAddressOf());
	UINT ViewportCount = 1;
	D3D11_VIEWPORT SavedViewport{};
	Context->RSGetViewports(&ViewportCount, &SavedViewport);

	bool bFilled = false;
	{
		SCOPE_CYCLE_COUNTER(STAT_GPUOcclusionPrepare);
		bFilled = FillItems(Proxies, Count, View);
		if (bFilled)
		{
			DrawOccluders(Proxies, View);
			BuildHiZ();
			Cull(View.ViewProjection, Count);
		}
	}

	Context->OMSetRenderTargets(1, SavedRTV.GetAddressOf(), SavedDSV.Get());
	if (ViewportCount > 0)
		Context->RSSetViewports(1, &SavedViewport);

	return bFilled && ReadBack(Count);
}
