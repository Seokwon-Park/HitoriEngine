#pragma once

#include "Render/RenderPacket.h"
#include "Render/PipelineState.h"
#include "Render/ObjectConstants.h"
#include "Render/RenderEnums.h"
#include "Render/Buffer.h"
#include "Render/RenderCommand.h"
#include <array>
#include <vector>

struct FSceneView;
struct FSceneRenderData;

// 한 패스에서 함께 바인딩할 GPU 상태. UObject 메시/재질 포인터는 보관하지 않는다.
struct FMeshDrawBatch
{
	struct FTextureBinding
	{
		UTexture2D* Texture = nullptr;
		EShaderBindFlagBits Stages = EShaderBindFlagBits::Pixel;
	};

	struct FItem
	{
		std::array<uint8, sizeof(FForwardObjectConstants)> ObjectData{};
		uint32 ObjectDataSize = 0;
		uint32 ObjectSlot = InvalidObjectSlot;
		uint32 StartIndex = 0;
		uint32 IndexCount = 0;
		const void* ObjectKey = nullptr;
		bool bOccludedByGpu = false;
	};

	FPipelineState Pipeline;

	FVertexBuffer* VertexBuffer = nullptr;
	FIndexBuffer* IndexBuffer = nullptr;

	std::vector<FTextureBinding> Textures;
	ESamplerState Sampler = ESamplerState::LinearClamp;
	EShaderBindFlagBits SamplerStages = EShaderBindFlagBits::Pixel;
	std::array<uint8, sizeof(FForwardMaterialConstants)> MaterialData{};
	uint32 MaterialDataSize = 0;
	uint32 MaterialSlot = 1;
	EShaderBindFlagBits MaterialStages = EShaderBindFlagBits::Pixel;
	uint32 ObjectBindingSlot = 2;
	EShaderBindFlagBits ObjectStages = EShaderBindFlagBits::Vertex;
	bool bUseObjectSlots = true;
	bool bBindViewConstants = true;
	bool bForceWireframe = false;
	std::vector<FItem> Items;
};

struct FOcclusionMeasureResult
{
	bool bValid = false;
	uint32 TotalObjects = 0;
	uint32 VisibleObjects = 0;
	uint32 TotalDraws = 0;
	uint32 VisibleDraws = 0;
	uint64 TotalTriangles = 0;
	uint64 VisibleTriangles = 0;
	double ElapsedMs = 0.0;
	uint32 GPUOccludedDraws = 0;
	uint32 FalseCulls = 0;
};

enum class EMeshPassType : uint8
{
	Opaque,
	GBuffer,
	Forward,
	Translucent,
	WorldNormal,
	CSMShadowDepth,
};

// CPU에서 프록시/패킷을 패스별 GPU 배치로 변환한다. GPU 명령은 호출하지 않는다.
class FMeshPassProcessor
{
public:
	bool Build(const FSceneView& View, const FRenderQueue& Queue,
		const FSceneRenderData* SceneData, EMeshPassType Pass,
		TArray<FMeshDrawBatch>& Out, ELightingModel LightingModel = ELightingModel::Gouraud) const;
};
