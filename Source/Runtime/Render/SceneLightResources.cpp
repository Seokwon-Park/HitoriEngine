#include "EnginePCH.h"
#include "Render/SceneLightResources.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"
#include "Components/LightComponentBase.h"
#include "Render/ShadowRendering.h"
#include <algorithm>
#include <cstring>

namespace
{
	static_assert(offsetof(FDeferredLightConstants, ViewportSize) == 96);
	static_assert(offsetof(FDeferredLightConstants, Ambient) == 112);
	static_assert(offsetof(FDeferredLightConstants, LightCounts) == 128);
	static_assert(offsetof(FDeferredLightConstants, LightOffsets) == 144);
	static_assert(offsetof(FDeferredLightConstants, SkyColorIntensity) == 160);
	static_assert(sizeof(FDeferredLightConstants) == 176);

	// 용량이 모자랄 때만 두 배로 키우고 Map 한 번으로 채운다.
	bool UploadStructured(TUniquePtr<FStructuredBuffer>& Buffer, const void* Data, uint32 Count, uint32 Stride)
	{
		if (!Count) return true;
		if (!Buffer || Buffer->GetCapacity() < Count)
		{
			const uint32 PreviousCapacity = Buffer ? Buffer->GetCapacity() : 0;
			const uint32 Maximum = UINT32_MAX / Stride;
			const uint32 Doubled = PreviousCapacity <= Maximum / 2
				? std::max(1u, PreviousCapacity * 2) : Maximum;
			auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(std::max(Count, Doubled), Stride);
			if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
			Buffer = std::move(NewBuffer);
		}
		void* Destination = RenderCommand::MapWriteDiscard(Buffer.get());
		if (!Destination) return false;
		std::memcpy(Destination, Data, static_cast<size_t>(Count) * Stride);
		RenderCommand::Unmap(Buffer.get());
		return true;
	}
}

bool FSceneLightResources::Init()
{
	if (bInitialized) return true;
	ForwardLightCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardLightingConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	ShadowProjectionLightCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowProjectionConstants));
	if (!ForwardTileGrid.Init())
		HTR_LOG(Warning, "Forward tile culling compute shader unavailable");

	bInitialized = ForwardLightCB && DeferredLightCB;
	return bInitialized;
}

bool FSceneLightResources::UploadViewLights(const TArray<FLightRenderParameters>& Lights,
	uint32 (&Counts)[4], uint32 (&Offsets)[4])
{
	// ELightType 순서(방향광, 포인트, 스포트, 렉트)가 곧 버퍼 안의 구간 순서다.
	constexpr uint32 TypeCount = 4;
	uint32 TypeCounts[TypeCount] = {};
	for (const FLightRenderParameters& Light : Lights)
		if (Light.Type < TypeCount) ++TypeCounts[Light.Type];

	const uint32 DirectionalType = static_cast<uint32>(ELightType::Directional);
	const uint32 PointType = static_cast<uint32>(ELightType::Point);
	const uint32 SpotType = static_cast<uint32>(ELightType::Spot);
	const uint32 RectType = static_cast<uint32>(ELightType::Rect);
	Counts[0] = TypeCounts[PointType];
	Counts[1] = TypeCounts[SpotType];
	Counts[2] = TypeCounts[DirectionalType];
	Counts[3] = TypeCounts[RectType];
	Offsets[2] = 0;
	Offsets[0] = Counts[2];
	Offsets[1] = Counts[2] + Counts[0];
	Offsets[3] = Counts[2] + Counts[0] + Counts[1];

	const uint32 Total = Counts[0] + Counts[1] + Counts[2] + Counts[3];
	if (!Total) return true;

	uint32 Next[TypeCount] = {};
	Next[DirectionalType] = Offsets[2];
	Next[PointType] = Offsets[0];
	Next[SpotType] = Offsets[1];
	Next[RectType] = Offsets[3];

	TArray<FLightRenderParameters> Packed;
	Packed.SetNum(static_cast<int32>(Total));
	for (const FLightRenderParameters& Light : Lights)
		if (Light.Type < TypeCount) Packed[Next[Light.Type]++] = Light;
	return UploadStructured(SceneLightBuffer, Packed.GetData(), Total, sizeof(FLightRenderParameters));
}

