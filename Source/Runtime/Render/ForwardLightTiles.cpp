#include "EnginePCH.h"
#include "ForwardLightTiles.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/SceneView.h"
#include "Render/Texture2D.h"
#include <array>
#include <cmath>

namespace
{
	ComPtr<ID3D11ComputeShader> CompileCullingShader(const wchar_t* Path)
	{
		ComPtr<ID3DBlob> Code, Errors;
		ComPtr<ID3D11ComputeShader> Shader;
		if (FAILED(D3DCompileFromFile(Path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainCS", "cs_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS, 0, Code.GetAddressOf(), Errors.GetAddressOf())))
		{
			if (Errors) OutputDebugStringA(static_cast<const char*>(Errors->GetBufferPointer()));
			return nullptr;
		}
		if (FAILED(RenderCommand::GetDevice()->CreateComputeShader(Code->GetBufferPointer(),
			Code->GetBufferSize(), nullptr, Shader.GetAddressOf()))) return nullptr;
		return Shader;
	}
}

bool FForwardLightTileGrid::FCellBuffer::EnsureCapacity(uint32 Count, uint32 Stride)
{
	if (Buffer && Count <= Capacity) return true;
	if (!Count || Count > UINT32_MAX / Stride) return false;
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = Count * Stride;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;
	ID3D11Device* Device = RenderCommand::GetDevice();
	ComPtr<ID3D11Buffer> NewBuffer;
	ComPtr<ID3D11ShaderResourceView> NewSRV;
	ComPtr<ID3D11UnorderedAccessView> NewUAV;
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, NewBuffer.GetAddressOf()))) return false;
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.NumElements = Count;
	if (FAILED(Device->CreateShaderResourceView(NewBuffer.Get(), &SRVDesc, NewSRV.GetAddressOf()))) return false;
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.NumElements = Count;
	if (FAILED(Device->CreateUnorderedAccessView(NewBuffer.Get(), &UAVDesc, NewUAV.GetAddressOf()))) return false;
	Buffer = std::move(NewBuffer);
	SRV = std::move(NewSRV);
	UAV = std::move(NewUAV);
	Capacity = Count;
	return true;
}

bool FForwardLightTileGrid::Init()
{
	CullingCB = RenderCommand::CreateConstantBuffer(sizeof(FLightCullingConstants));
	GridCB = RenderCommand::CreateConstantBuffer(sizeof(FLightGridConstants));
	if (!CullingCB || !GridCB) return false;
	CellCullingShader = CompileCullingShader(L"Resources/Shader/TileBasedLightCulling.hlsl");
	Tiled25DShader = CompileCullingShader(L"Resources/Shader/TiledLightCulling25D.hlsl");
	return CellCullingShader && Tiled25DShader;
}

bool FForwardLightTileGrid::PrepareBuild(const FSceneView& View, const FForwardLightingConstants& Lighting,
	const FStructuredBuffer* SceneLights, const FTexture2D* Depth, ELightCullingMode InMode)
{
	bPixelTilesValid = false;
	Mode = ELightCullingMode::Off;
	CullingConstants = {};
	GridConstants = {};

	// SceneLights는 방향광, 포인트, 스포트, 렉트 순으로 이어져 있어 포인트부터 렉트까지가 한 구간이다.
	const uint32 LocalLightOffset = Lighting.LightOffsets[0];
	const uint32 NumLocalLights = Lighting.LightCounts[0] + Lighting.LightCounts[1] + Lighting.LightCounts[3];
	GridConstants.NumLocalLights = NumLocalLights;

	if (!CullingCB || !GridCB || !Depth || !Depth->GetWidth() || !Depth->GetHeight()) return false;
	if (InMode == ELightCullingMode::Off || !NumLocalLights) return true;

	const bool b25D = InMode == ELightCullingMode::Tiled25D;
	const bool bClustered = InMode == ELightCullingMode::Clustered;
	ID3D11ComputeShader* Shader = b25D ? Tiled25DShader.Get() : CellCullingShader.Get();
	if (!Shader || !SceneLights || !SceneLights->GetSRV() || (b25D && !Depth->GetSRV()))
		return false;
	if (!std::isfinite(View.NearClip) || !std::isfinite(View.FarClip) ||
		View.NearClip <= 0.0f || View.FarClip <= View.NearClip)
		return false;

	const uint32 Width = Depth->GetWidth();
	const uint32 Height = Depth->GetHeight();
	const uint32 TileSize = bClustered ? ClusteredTileSize : TiledTileSize;
	const uint32 NumSlices = bClustered ? ClusteredSlices : 1;
	const uint32 NumTilesX = (Width + TileSize - 1) / TileSize;
	const uint32 NumTilesY = (Height + TileSize - 1) / TileSize;
	const uint64 CellCount = static_cast<uint64>(NumTilesX) * NumTilesY * NumSlices;
	if (CellCount > UINT32_MAX / CellStride ||
		!CellLightIndices.EnsureCapacity(static_cast<uint32>(CellCount) * CellStride, sizeof(uint32)))
		return false;

	const bool bOrtho = !View.bIsPerspective;
	const float NearZ = View.NearClip;
	const float FarZ = View.FarClip;

	CullingConstants.View = View.ViewMatrix;
	CullingConstants.Projection = View.ProjectionMatrix;
	CullingConstants.ViewportSize = FVector2(static_cast<float>(Width), static_cast<float>(Height));
	CullingConstants.NumTilesX = NumTilesX;
	CullingConstants.NumTilesY = NumTilesY;
	CullingConstants.NumLocalLights = NumLocalLights;
	CullingConstants.NumSlices = NumSlices;
	CullingConstants.TileSize = TileSize;
	CullingConstants.bOrthographic = bOrtho ? 1u : 0u;
	CullingConstants.NearZ = NearZ;
	CullingConstants.FarZ = FarZ;
	CullingConstants.LocalLightOffset = LocalLightOffset;

	// 원근은 로그 분포라 PS가 Slice = log(z) * SliceScale + SliceBias로 바로 구하게 미리 계산한다. 직교는 선형이다.
	GridConstants.NumTilesX = NumTilesX;
	GridConstants.NumTilesY = NumTilesY;
	GridConstants.NumSlices = NumSlices;
	GridConstants.NearZ = NearZ;
	GridConstants.FarZ = FarZ;
	GridConstants.SliceScale = bOrtho ? static_cast<float>(NumSlices) / (FarZ - NearZ)
		: static_cast<float>(NumSlices) / std::log(FarZ / NearZ);
	GridConstants.SliceBias = bOrtho ? -NearZ * GridConstants.SliceScale : -std::log(NearZ) * GridConstants.SliceScale;
	GridConstants.TileSize = TileSize;
	GridConstants.bOrthographic = bOrtho ? 1u : 0u;
	GridConstants.ProjZScale = View.ProjectionMatrix.M[0][2];
	GridConstants.ProjZOffset = View.ProjectionMatrix.M[3][2];

	// 모든 준비가 끝났을 때만 요청한 모드를 활성화한다.
	Mode = InMode;
	return true;
}

bool FForwardLightTileGrid::ExecuteBuild(FRenderer* Renderer, FTexture2D* Depth, const FStructuredBuffer* SceneLights)
{
	const bool b25D = Mode == ELightCullingMode::Tiled25D;
	ID3D11ComputeShader* Shader = b25D ? Tiled25DShader.Get() : CellCullingShader.Get();
	if (Mode == ELightCullingMode::Off || !Renderer || !Shader || !SceneLights || !SceneLights->GetSRV() ||
		!CellLightIndices.UAV || (b25D && (!Depth || !Depth->GetSRV())))
		return false;

	Renderer->UpdateConstant(CullingCB.get(), &CullingConstants, sizeof(CullingConstants));

	// t10: LocalLights, t11: DepthTexture(2.5D 전용), u0: TileLightIndices
	const std::array<ID3D11ShaderResourceView*, 2> Inputs = { SceneLights->GetSRV(), b25D ? Depth->GetSRV() : nullptr };
	const std::array<ID3D11UnorderedAccessView*, 1> Outputs = { CellLightIndices.UAV.Get() };
	Renderer->DispatchCompute(Shader, CullingCB->GetBuffer(), 10, Inputs, Outputs,
		CullingConstants.NumTilesX, CullingConstants.NumTilesY, CullingConstants.NumSlices);
	return true;
}

void FForwardLightTileGrid::PreparePixelShader(FRenderer* Renderer, bool bBuildSucceeded)
{
	bPixelTilesValid = Mode != ELightCullingMode::Off && bBuildSucceeded && CellLightIndices.SRV;
	if (!Renderer || !GridCB) return;

	// 목록을 못 만들었으면 Off로 올려 PS가 전체 로컬 광원을 순회하게 한다.
	FLightGridConstants PixelConstants = GridConstants;
	PixelConstants.GridMode = static_cast<uint32>(bPixelTilesValid ? Mode : ELightCullingMode::Off);
	Renderer->UpdateConstant(GridCB.get(), &PixelConstants, sizeof(PixelConstants));
}
