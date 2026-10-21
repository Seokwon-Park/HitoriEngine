#pragma once

#include "Core/Types.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

class FTexture2D;
class FRenderGraphBuilder;

// 핸들은 이 그래프의 Execute가 끝날 때까지만 유효하다. 텍스처의 소유권은 갖지 않는다.
struct FRenderGraphTextureRef
{
	static constexpr uint32 InvalidIndex = UINT32_MAX;
	uint32 Index = InvalidIndex;
	const FRenderGraphBuilder* Owner = nullptr;
	explicit operator bool() const { return Owner && Index != InvalidIndex; }
};

enum class ERenderGraphLoadAction : uint8
{
	Load,
	Clear,
};

struct FRenderGraphColorAttachment // 패스에 필요한 ColorTextrue 핸들
{
	FRenderGraphTextureRef Texture;
	ERenderGraphLoadAction LoadAction = ERenderGraphLoadAction::Load;
	std::array<float, 4> ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
};

struct FRenderGraphDepthAttachment // 패스에 필요한 DepthTextrue 핸들
{
	FRenderGraphTextureRef Texture;
	ERenderGraphLoadAction LoadAction = ERenderGraphLoadAction::Load;
	float ClearDepth = 1.0f;
	uint8 ClearStencil = 0;
};

struct FRenderGraphRasterPassDesc // draw pass용
{
	std::string Name;
	std::vector<FRenderGraphTextureRef> ReadTextures;
	std::vector<FRenderGraphColorAttachment> ColorAttachments;
	FRenderGraphDepthAttachment DepthAttachment;
	uint32 Width = 0;  // 0이면 첫 출력 텍스처의 크기를 사용한다.
	uint32 Height = 0;
};

struct FRenderGraphComputePassDesc // compute pass용
{
	std::string Name;
	std::vector<ID3D11Resource*> ReadResources;
	std::vector<ID3D11Resource*> WriteResources;
};

struct FRenderGraphTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	UINT BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
};

// 1단계: 외부 텍스처와 순차 래스터 패스만 지원한다.
// 패스의 실행 함수는 바인딩된 RTV/DSV를 변경하지 않아야 한다.
class FRenderGraphBuilder
{
public:
	FRenderGraphBuilder() = default;
	~FRenderGraphBuilder();
	FRenderGraphBuilder(const FRenderGraphBuilder&) = delete;
	FRenderGraphBuilder& operator=(const FRenderGraphBuilder&) = delete;

	FRenderGraphTextureRef ImportTexture(FTexture2D* Texture, const char* Name);
	FRenderGraphTextureRef CreateTexture(const FRenderGraphTextureDesc& Desc, const char* Name);
	FTexture2D* Resolve(FRenderGraphTextureRef Ref) const;

	bool AddRasterPass(FRenderGraphRasterPassDesc Desc, std::function<void()> Execute);
	bool AddComputePass(FRenderGraphComputePassDesc Desc, std::function<void()> Execute);
	bool Execute();

private:
	struct FImportedTexture
	{
		FTexture2D* Texture = nullptr;
		std::string Name;
		FRenderGraphTextureDesc Desc{};
		bool bTransient = false;
	};
	struct FRasterPass
	{
		FRenderGraphRasterPassDesc Desc;
		FRenderGraphComputePassDesc ComputeDesc;
		std::function<void()> Execute;
		uint32 Width = 0;
		uint32 Height = 0;
		bool bCompute = false;
	};

	bool ValidatePass(const FRenderGraphRasterPassDesc& Desc, uint32& OutWidth, uint32& OutHeight) const;

	std::vector<FImportedTexture> Textures;
	std::vector<FRasterPass> Passes;
	bool bExecuted = false;
};
