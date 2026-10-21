#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"
#include <cstring>
#include <fstream>
#include <iterator>



struct FPipelineTableEntry
{
	EPSOType Type;
	const char* ShaderPath;
	D3D11_PRIMITIVE_TOPOLOGY Topology;
	ERasterizerState RasterizerState;
	EBlendState BlendState;
	EDepthStencilState DepthStencilState;
};

constexpr FPipelineTableEntry PipelineTable[] =
{
	{ EPSOType::StaticMesh_Opaque,       "Resources/Shader/StaticMeshShader.hlsl",            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::StaticMesh_Translucent,  "Resources/Shader/StaticMeshTranslucentShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::StaticMesh_Wireframe,    "Resources/Shader/StaticMeshShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::Wireframe, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::Particle_AlphaBlend,     "Resources/Shader/ParticleSubUVShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::Particle_Additive,       "Resources/Shader/ParticleSubUVShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Additive,     EDepthStencilState::ReadOnly },
	{ EPSOType::Skybox,                  "Resources/Shader/SkyboxShader.hlsl",        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::ReadOnly },
	{ EPSOType::Grid,                    "Resources/Shader/GridShader.hlsl",          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::Default },
	{ EPSOType::Outline_Mask,            "Resources/Shader/OutlineShader.hlsl",       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::NoColorWrite, EDepthStencilState::StencilMask },
	{ EPSOType::Outline_Draw,            "Resources/Shader/OutlineShader.hlsl",       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::StencilOutline },
	{ EPSOType::StaticMesh_GBuffer,		 "Resources/Shader/StaticMeshGBufferShader.hlsl",	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	ERasterizerState::SolidBack,	EBlendState::Opaque,  EDepthStencilState::Default },
	{ EPSOType::DeferredLighting,       "Resources/Shader/DeferredLightingShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Disabled },
	{ EPSOType::DeferredPointLighting,  "Resources/Shader/DeferredLightingShader.hlsl",    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Additive,     EDepthStencilState::Disabled },
	{ EPSOType::ToneMap,                "Resources/Shader/ToneMapShader.hlsl",              D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Disabled },
	{ EPSOType::Removed_FireBall,       nullptr,                                            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::Billboard,              "Resources/Shader/BillboardShader.hlsl",            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::StaticMesh_Forward,		"Resources/Shader/UberLit.hlsl",			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,			ERasterizerState::SolidBack, EBlendState::Opaque, EDepthStencilState::Default },
	{ EPSOType::StaticMesh_WorldNormal, "Resources/Shader/UberLit.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::Opaque, EDepthStencilState::Default },
	{ EPSOType::StaticMesh_DepthOnly, "Resources/Shader/DepthPrepass.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidBack, EBlendState::NoColorWrite, EDepthStencilState::Default },
	{ EPSOType::LightCullingDebug, "Resources/Shader/LightCullingDebug.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::AlphaBlend, EDepthStencilState::Disabled },
	{ EPSOType::ShadowProjection, "Resources/Shader/ShadowProjectionPixelShader.hlsl", D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, ERasterizerState::SolidNone, EBlendState::Opaque, EDepthStencilState::Disabled },
};

static_assert(sizeof(PipelineTable) / sizeof(PipelineTable[0]) == static_cast<size_t>(EPSOType::Count));

void FRenderResourceManager::InitPipelineStates()
{
	for (size_t i = 0; i < static_cast<size_t>(EPSOType::Count); ++i)
	{
		const auto& Entry = PipelineTable[i];
		const EPSOType Type = static_cast<EPSOType>(i);
		PipelineStateMap[Type] = MakeUnique<FPipelineState>();
		FPipelineState* State = PipelineStateMap[Type].get();
		// World Normal은 전용 매크로 변형이 성공했을 때만 연결한다.
		// 셰이더 경로가 없는 칸(삭제된 PSO)은 셰이더 없이 둔다.
		State->Shader = Type == EPSOType::StaticMesh_WorldNormal || !Entry.ShaderPath ? nullptr : GetShaderProgram(Entry.ShaderPath);
		State->Topology = Entry.Topology;
		State->RasterizerState = Entry.RasterizerState;
		State->BlendState = Entry.BlendState;
		State->DepthStencilState = Entry.DepthStencilState;
	}
	const char* Names[] = { "LIGHTING_MODEL_GOURAUD", "LIGHTING_MODEL_LAMBERT", "LIGHTING_MODEL_PHONG", "VIEW_MODE_WORLD_NORMAL" };
	for (const char* Name : Names)
	{
		const D3D_SHADER_MACRO Defines[] = { { Name, "1" }, { nullptr, nullptr } };
		const FString Path = "Resources/Shader/UberLit.hlsl";
		FShaderByteCode VSCode = RenderUtil::CompileShader(Path, "mainVS", EShaderType::Vertex, Defines);
		FShaderByteCode PSCode = RenderUtil::CompileShader(Path, "mainPS", EShaderType::Pixel, Defines);
		if (!VSCode.IsValid() || !PSCode.IsValid())
		{
			HTR_LOG(Error, "[Shader] UberShader variant compile failed: {}", Name);
			continue;
		}
		auto VS = RenderCommand::CreateVertexShader(VSCode);
		auto PS = RenderCommand::CreatePixelShader(PSCode);
		if (!VS || !VS->IsValid() || !PS || !PS->IsValid())
		{
			continue;
		}
		FVertexShader* VSRaw = VS.get();
		FPixelShader* PSRaw = PS.get();
		const FString Key = Path + "#" + Name;
		VertexShaderMap[Key + "#VS"] = std::move(VS);
		PixelShaderMap[Key + "#PS"] = std::move(PS);
		ShaderProgramMap[Key] = MakeUnique<FShaderProgram>(VSRaw, PSRaw);
		if (std::strcmp(Name, "VIEW_MODE_WORLD_NORMAL") == 0)
			PipelineStateMap[EPSOType::StaticMesh_WorldNormal]->Shader = ShaderProgramMap[Key].get();
	}
}

FShaderProgram* FRenderResourceManager::GetForwardShaderProgram(ELightingModel Model)
{
	const char* Name = "LIGHTING_MODEL_LAMBERT";
	switch (Model)
	{
	case ELightingModel::Gouraud: Name = "LIGHTING_MODEL_GOURAUD"; break;
	case ELightingModel::Phong: Name = "LIGHTING_MODEL_PHONG"; break;
	default: break;
	}
	const FString Key = FString("Resources/Shader/UberLit.hlsl#") + Name;
	if (auto* Found = Get().ShaderProgramMap.Find(Key)) return Found->get();
	return nullptr;
}

FPipelineState* FRenderResourceManager::GetPSO(const EPSOType& InType)
{
	if (auto* Found = Get().PipelineStateMap.Find(InType))
	{
		return Found->get();
	}
	return nullptr;
}

void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	if (!fs::exists(ShaderRoot, ErrorCode))
	{
		HTR_LOG(Error, "[Shader] Scan Root Not Found : {}", ShaderRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;
		// 컴퓨트 전용 .hlsl에는 mainVS/mainPS가 없다. VS/PS로 컴파일하지 않는다.
		std::ifstream SourceFile(Entry.path(), std::ios::binary);
		if (!SourceFile) continue;
		const std::string Source(std::istreambuf_iterator<char>{ SourceFile }, std::istreambuf_iterator<char>{});
		if (Source.find("mainVS(") == std::string::npos ||
			Source.find("mainPS(") == std::string::npos) continue;

		FString Path = Entry.path().generic_string();
		LoadOrCompileShader(Path);
	}
}

void FRenderResourceManager::Shutdown()
{
	Get().VertexShaderMap.Empty();
	Get().PixelShaderMap.Empty();
	Get().ShaderProgramMap.Empty();
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath)
{
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(InPath))
		return Found->get();

	HTR_LOG(Error, "[Shader] not found: {}", InPath);

	if (TUniquePtr<FShaderProgram>* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->get();

	return nullptr;
}



void FRenderResourceManager::LoadOrCompileShader(const FString& Path)
{ 
	FString VSCSOPath;
	FString PSCSOPath;
	FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath);
	FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath);

	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		HTR_LOG(Error, "[Shader] compile failed: {}", Path);
		return;
	}

	TUniquePtr<FVertexShader> Vs = RenderCommand::CreateVertexShader(VSCode);
	TUniquePtr<FPixelShader>  Ps = RenderCommand::CreatePixelShader(PSCode);

	if (!Vs || !Vs->IsValid() || !Ps || !Ps->IsValid())
	{
		HTR_LOG(Error, "[Shader] device create failed: {}", Path);
		return;
	}

	FVertexShader* VsRaw = Vs.get();
	FPixelShader* PsRaw = Ps.get();

	VertexShaderMap[VSCSOPath] = std::move(Vs);
	PixelShaderMap[PSCSOPath] = std::move(Ps);
	ShaderProgramMap[Path] = MakeUnique<FShaderProgram>(VsRaw, PsRaw);

	HTR_LOG(Info, "[Shader] loaded: {}", Path);

}
