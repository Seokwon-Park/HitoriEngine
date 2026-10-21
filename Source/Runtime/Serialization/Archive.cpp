#include "EnginePCH.h"

#include "UObject/Object.h"

void FArchive::SerializeBool(bool& D)
{
	bool BoolValue = D;
	Serialize(&BoolValue, sizeof(BoolValue));

	if (IsLoading())
	{
		D = BoolValue;
	}
}

FArchive& operator<<(FArchive& Ar, FString& Value)
{
	int32 Num = static_cast<int32>(Value.size());
	Ar << Num;
	if (Ar.IsLoading())
	{
		if (Num < 0)
		{
			Ar.SetError();
			return Ar;
		}
		Value.resize(Num);
	}
	if (Num > 0)
		Ar.Serialize(Value.data(), Num);
	return Ar;
}

void FArchiveState::SetError()
{
	ArIsError = true;
}

void FArchiveState::SetIsLoading(bool bInIsLoading)
{
	ArIsLoading = bInIsLoading;
}

void FArchiveState::SetIsSaving(bool bInIsSaving)
{
	ArIsSaving = bInIsSaving;
}

void FArchiveState::SetIsTextFormat(bool bInIsTextFormat)
{
	ArIsTextFormat = bInIsTextFormat;
}

void FArchiveState::SetIsPersistent(bool bInIsPersistent)
{
	ArIsPersistent = bInIsPersistent;
}