#pragma once


#include <cstring>
#include <climits>
#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Core/EngineLog.h"
#include "Containers/Array.h"
#include "MemoryArchive.h"
#include "Math/EngineMath.h"
#include "Core/HAL/UnrealMemory.h"

class FMemoryReader : public FMemoryArchive
{
public:
	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override
	{
		return "FMemoryReader";
	}

	virtual int64 TotalSize() override
	{
		return std::min((int64)Bytes.Num(), LimitSize);
	}

	void Serialize(void* Data, int64 Num)
	{
		if (Num && !IsError())
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= TotalSize())
			{
				FMemory::Memcpy(Data, &Bytes[(int32)Offset], Num);
				Offset += Num;
			}
			else
			{
				SetError();
			}
		}
	}

	explicit FMemoryReader(const TArray<uint8>& InBytes, bool bIsPersistent = false)
		: Bytes(InBytes)
		, LimitSize(INT64_MAX)
	{
		SetIsLoading(true);
	}

	/** With this method it's possible to attach data behind some serialized data. */ 
	void SetLimitSize(int64 NewLimitSize)
	{
		LimitSize = NewLimitSize;
	}

private:
	const TArray<uint8>& Bytes;
	int64                LimitSize;
};