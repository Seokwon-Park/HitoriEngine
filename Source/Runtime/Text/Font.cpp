#include "EnginePCH.h"
#include "Font.h"

bool UFont::LoadFontAtlasJson(const FString& JsonPath)
{
	std::ifstream File(JsonPath);
	if (!File.is_open())
	{
		return false;
	}

	json Root;
	File >> Root;

	const auto& AtlasJson = Root["atlas"];
	Width = AtlasJson["width"].get<float>();
	Height = AtlasJson["height"].get<float>();
	DistanceRange = AtlasJson["distanceRange"].get<float>();
	LineHeight = Root["metrics"]["lineHeight"].get<float>();

	// yOrigin 체크: "bottom"이 아니면 V 계산 로직을 반대로 해야 함
	bool bYOriginBottom = AtlasJson.value("yOrigin", "bottom") == "bottom";

	for (const auto& GlyphJson : Root["glyphs"])
	{
		FGlyphInfo Glyph;
		uint32 Unicode = GlyphJson["unicode"].get<uint32>();
		Glyph.Advance = GlyphJson["advance"].get<float>();

		if (GlyphJson.contains("planeBounds") && GlyphJson.contains("atlasBounds"))
		{
			const auto& PB = GlyphJson["planeBounds"];
			Glyph.PlaneLeft = PB["left"].get<float>();
			Glyph.PlaneBottom = PB["bottom"].get<float>();
			Glyph.PlaneRight = PB["right"].get<float>();
			Glyph.PlaneTop = PB["top"].get<float>();

			const auto& AB = GlyphJson["atlasBounds"];
			Glyph.AtlasLeft = AB["left"].get<float>();
			Glyph.AtlasBottom = AB["bottom"].get<float>();
			Glyph.AtlasRight = AB["right"].get<float>();
			Glyph.AtlasTop = AB["top"].get<float>();

			Glyph.bHasBounds = true;
		}

		GlyphMap.Add(Unicode, Glyph);
	}

	// kerning (선택 사항 - 있으면 파싱)
	if (Root.contains("kerning"))
	{
		for (const auto& KerningJson : Root["kerning"])
		{
			uint32 U1 = KerningJson["unicode1"].get<uint32>();
			uint32 U2 = KerningJson["unicode2"].get<uint32>();
			float Adv = KerningJson["advance"].get<float>();

			uint64 Key = ((uint64)U1 << 32) | U2;
			KerningMap.Add(Key, Adv);
		}
	}

	return true;
}
