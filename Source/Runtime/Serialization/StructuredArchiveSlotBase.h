#pragma once

#include "Core/Types.h"

class FArchive;
class FStructuredArchive;

struct FElementId
{
	uint32 Id = 0;
	bool IsValid() const { return Id != 0; }
	void Reset() { Id = 0; }
	bool operator==(const FElementId& Other) const { return Id == Other.Id; }
	bool operator!=(const FElementId& Other) const { return Id != Other.Id; }
};

class FSlotBase
{
	friend class FStructuredArchive;
public:
	FArchive& GetUnderlyingArchive() const;

protected:
	FSlotBase(FStructuredArchive& InAr, int32 InDepth, FElementId InId)
		:StructuredArchive(InAr), Depth(InDepth), ElementId(InId) {}

	FStructuredArchive& StructuredArchive;
	int32 Depth;
	FElementId ElementId;
};