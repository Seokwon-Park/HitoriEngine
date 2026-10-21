#include "EnginePCH.h"
#include "RenderUtil.h"

namespace
{
	const char* GetShaderTarget(EShaderType Type)
	{
		switch (Type)
		{
		case EShaderType::Vertex:
			return "vs_5_0";
			break;
		case EShaderType::Pixel:
			return "ps_5_0";
			break;
		case EShaderType::Compute:
			return "cs_5_0";
			break;
		default:
			return "";
			break;
		}
	}

	const char* GetShaderPrefix(EShaderType Type)
	{
		switch (Type)
		{
		case EShaderType::Vertex:
			return "_VS.cso";
			break;
		case EShaderType::Pixel:
			return  "_PS.cso";
			break;
		case EShaderType::Compute:
			return  "_CS.cso";
			break;
		default:
			return "";
			break;
		}
	}
}

FShaderByteCode RenderUtil::CompileShader(FString Path, const char* EntryPoint, EShaderType ShaderType, const D3D_SHADER_MACRO* Macros)
{
	const char* Target = GetShaderTarget(ShaderType);

	uint32 Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	FShaderByteCode ByteCode;

	std::wstring WPath(Path.begin(), Path.end());

	//for (const auto& D : Defines)
	//	Macros.push_back({ D.first.c_str(), D.second.c_str() });
	//Macros.push_back({ nullptr, nullptr });		// 끝 표시

	ComPtr<ID3DBlob> ErrorBlob;
	HRESULT hr = D3DCompileFromFile(WPath.c_str(), Macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint, Target, Flags, 0, ByteCode.Blob.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (FAILED(hr))
	{
		if (ErrorBlob)
			OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		else
			OutputDebugStringA(("[Shader] file not found: " + Path + "\n").c_str());

		ByteCode.Blob.Reset();
	}
	return ByteCode;
}

void RenderUtil::SaveByteCode(FString CsoPath, const FShaderByteCode& ByteCode)
{
	fs::create_directories(fs::path(CsoPath).parent_path());
	std::ofstream Out(CsoPath, std::ios::binary);
	if (!Out) { fs::remove(CsoPath); return; }
	Out.write(reinterpret_cast<const char*>(ByteCode.GetData()), ByteCode.GetSize());
}

FShaderByteCode RenderUtil::LoadByteCode(const FString& CsoPath)
{
	FShaderByteCode Result;

	std::ifstream In(CsoPath, std::ios::binary | std::ios::ate);
	if (!In) return Result;

	const std::streamsize Size = In.tellg();
	In.seekg(0);
	if (Size <= 0) return Result;

	ComPtr<ID3DBlob> Blob;
	if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(Size), &Blob))) return Result;

	In.read(reinterpret_cast<char*>(Blob->GetBufferPointer()), Size);
	if (In) Result.Blob = std::move(Blob);
	return Result;
}

FString RenderUtil::GetCsoPath(const FString& ShaderPath, EShaderType ShaderType, const D3D_SHADER_MACRO* Macros)
{
	const fs::path Rel = fs::relative(ShaderPath, "Resources/Shader");
	const char* Suffix = GetShaderPrefix(ShaderType);
	FString MacroSuffix;
	while (Macros && Macros->Name)
	{
		MacroSuffix += FString("_") + Macros->Name;
		Macros++;
	}

	fs::path Out = fs::path("Intermediate/Shaders") / Rel.parent_path();
	Out /= Rel.stem().generic_string() + MacroSuffix.c_str() + Suffix;
	return Out.generic_string();
}

// .cso 파일이 최신 컴파일인지 비교한다.
bool RenderUtil::IsCsoUpToDate(const FString& ShaderPath, const FString& CsoPath)
{
	std::ifstream SourceFile{ ShaderPath };
	const std::string Source{ std::istreambuf_iterator<char>{ SourceFile }, std::istreambuf_iterator<char>{} };
	if (Source.find("#include") != std::string::npos) {
		return false;
	}

	std::error_code Ec;
	if (!fs::exists(CsoPath, Ec)) return false;

	const auto SrcTime = fs::last_write_time(ShaderPath, Ec);
	if (Ec) return false;
	const auto CsoTime = fs::last_write_time(CsoPath, Ec);
	if (Ec) return false;

	return CsoTime > SrcTime;
}

FShaderByteCode RenderUtil::GetOrCompile(const FString& ShaderPath, const char* EntryPoint, EShaderType ShaderType, FString& OutCSOPath, const D3D_SHADER_MACRO* Macros)
{
	OutCSOPath = GetCsoPath(ShaderPath, ShaderType, Macros);

	if (IsCsoUpToDate(ShaderPath, OutCSOPath))
	{
		FShaderByteCode Cached = LoadByteCode(OutCSOPath);
		if (Cached.IsValid()) return Cached;
	}

	FShaderByteCode Compiled = CompileShader(ShaderPath, EntryPoint, ShaderType, Macros);
	if (Compiled.IsValid())
		SaveByteCode(OutCSOPath, Compiled);

	return Compiled;

}
