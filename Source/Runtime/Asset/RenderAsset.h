#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class URenderAsset : public UObject
{
	DECLARE_CLASS(URenderAsset, UObject)
public:
	URenderAsset() = default;
	virtual ~URenderAsset() = default;

	void SetPath(FString Path) { AssetPath = Path; }
	const FString& GetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
