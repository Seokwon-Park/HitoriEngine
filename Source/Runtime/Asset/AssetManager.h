#pragma once

#include "UObject/Object.h"
#include "UObject/Property.h"
#include "Render/Renderer.h"
#include "Asset/LOD/StaticMeshLODGenerator.h"

#include <filesystem>
#include <functional>

enum class EAssetType
{

};

class UStaticMesh;
class FShader;

namespace fs = std::filesystem;

class UAssetManager : public UObject
{
	DECLARE_CLASS(UAssetManager, UObject)
private:
	UAssetManager() = default;

	UAssetManager(const UAssetManager& src) =  delete;
	UAssetManager& operator= (const UAssetManager& src) = delete;

	using FAssetLoadProgress = std::function<void(int32 Loaded, int32 Total, const FString& Path)>;
public:
	static UAssetManager& Get();
	static void Init(const FAssetLoadProgress& OnProgress = nullptr);
	static void Shutdown();

	void ScanAssets(const fs::path& AssetRoot, const FAssetLoadProgress& OnProgress);
	void LoadAsset(const FString& Key, const FString& Path);

	void CreateDefaultTextures();
	void CreateDefaultMeshes();
	void CreateDefaultMaterial();
	void CreateBillboardMaterial();
	void CreateParticleMaterial();
	void CreateEditorArrowMaterial();

	template <typename T>
	static T* GetAssetByPath(const FString& Path)
	{
		URenderAsset** Found = Get().AssetMap.Find(Path);

		if (Found == nullptr || *Found == nullptr || !(*Found)->IsA<T>())
		{
			return nullptr;
		}
		return Cast<T>(*Found);
	}

	void RegisterAsset(const FString& Key, URenderAsset* Asset);

	//UStaticMesh* GetMesh(FString InName);
	//UStaticMesh* GetMesh(EPrimitiveType Type); // 오버로드(수정 중)

	UTexture2D* LoadTexture(const FString& InPath, bool bGenerateMips = true);
	UFont* LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath);
	static UStaticMesh* LoadObjStaticMesh(const FString& Path);

	FLODGenerateResult GenerateStaticMeshLODs(UStaticMesh& Mesh,const FLODGenerateRequest& Request);

	// 원본 좌표축이 엔진과 다른 메시를 로드 후 돌린다 (정점·법선·AABB·삼각형 BVH·GPU 정점 버퍼를 다시 만든다).
	// LOD는 LOD0에서 만들어지므로 LOD를 만들기 전에만 부를 수 있다. Rotate는 회전(길이·손잡이 보존)이어야 한다.
	static bool ReorientStaticMesh(UStaticMesh& Mesh, const std::function<FVector(const FVector&)>& Rotate);

	// 로드된 OBJ 스태틱 메시(키가 .obj로 끝나는 것)마다 부른다. 기본 도형·기즈모 메시는 제외된다.
	static void ForEachObjStaticMesh(const std::function<void(const FString& Key, UStaticMesh& Mesh)>& Func);

private:
	TMap<FString, FString> AssetPathMap;
	TMap<FString, URenderAsset*> AssetMap;

	//TMap<FString, UStaticMesh*> MeshMap;	// KEY: FILE NAME OR 쉐입 첫글자 대문자
	TMap<FString, TUniquePtr<FShader>> ShaderMap;	// KEY: FILE NAME

	//TMap<FString, UTexture2D*> TextureMap;
};
