#pragma once

#include "Asset/RenderAsset.h"

class UTexture2D;

struct FGlyphInfo // 글자 하나의 정보
{
	float Advance = 0.0f; // 이 글자를 그린 후 펜을 얼마나 오른쪽으로 옮길지 
	// 로컬 쿼드 좌표 (em 단위, 폰트 크기 1.0 기준) 
	float PlaneLeft = 0, PlaneBottom = 0, PlaneRight = 0, PlaneTop = 0;
	// 아틀라스 텍스처 내 픽셀 좌표 
	float AtlasLeft = 0, AtlasBottom = 0, AtlasRight = 0, AtlasTop = 0;
	bool bHasBounds = false; // 공백(unicode 32)처럼 그릴 게 없는 글자 구분용
};

class UFont : public URenderAsset
{
	DECLARE_CLASS(UFont, URenderAsset)
public:
	bool LoadFontAtlasJson(const FString& JsonPath);

	float Width = 0.0f; // 아틀라스 텍스처 가로 픽셀 (json: atlas.width)
	float Height = 0.0f; // 아틀라스 텍스처 세로 픽셀 (json: atlas.height)
	float DistanceRange = 2.0f;// MSDF distance range (json: atlas.distanceRange) - 셰이더에서 씀
	float LineHeight = 1.0f; // 줄바꿈 시 다음 줄로 내려갈 간격 (json: metrics.lineHeight)
	TMap<uint32, FGlyphInfo> GlyphMap; // unicode -> 그 글자의 정보 (모든 글리프를 담는 사전)
	TMap<uint64, float> KerningMap; // (Unicode1 << 32 | Unicode2) -> advance 보정값

	UTexture2D* AtlasTexture;

};


