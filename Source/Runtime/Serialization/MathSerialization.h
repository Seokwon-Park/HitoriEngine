#pragma once

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Serialization/StructuredArchiveSlots.h"

// UE Vector.h와 동일: Record {X, Y, Z}
inline void operator<<(FStructuredArchiveSlot Slot, FVector& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	Record << SA_VALUE("X", V.X);
	Record << SA_VALUE("Y", V.Y);
	Record << SA_VALUE("Z", V.Z);
}

// UE Vector2D.h와 동일: Stream (개수를 기록하지 않음)
inline void operator<<(FStructuredArchiveSlot Slot, FVector2& V)
{
	FStructuredArchiveStream Stream = Slot.EnterStream();
	Stream.EnterElement() << V.X;
	Stream.EnterElement() << V.Y;
}

// UE는 USTRUCT 리플렉션으로 멤버 이름을 키로 쓴다 -> 같은 결과가 나오게 Record로
inline void operator<<(FStructuredArchiveSlot Slot, FVector4& V)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	Record << SA_VALUE("X", V.X) << SA_VALUE("Y", V.Y) << SA_VALUE("Z", V.Z) << SA_VALUE("W", V.W);
}

inline void operator<<(FStructuredArchiveSlot Slot, FRotator& R)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	Record << SA_VALUE("Pitch", R.Pitch) << SA_VALUE("Yaw", R.Yaw) << SA_VALUE("Roll", R.Roll);
}

inline void operator<<(FStructuredArchiveSlot Slot, FTransform& T)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	Record << SA_VALUE("Location", T.Location) << SA_VALUE("Rotation", T.Rotation) << SA_VALUE("Scale", T.Scale);
}