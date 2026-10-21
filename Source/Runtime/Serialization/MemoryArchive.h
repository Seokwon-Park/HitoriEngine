#pragma once

#include "Archive.h"

class FMemoryArchive : public FArchive
{
public:
	virtual FString GetArchiveName() const { return "FMemoryArchilve"; }

	void Seek(int64 InPos) final { Offset = InPos; }
	int64 Tell() final { return Offset; }

	using FArchive::operator<<;

	virtual FArchive& operator<<(class FName& N) override
	{
		// Serialize the FName as a string
		if (IsLoading())
		{
			FString StringName;
			*this << StringName;
			N = FName(StringName);
		}
		else
		{
			FString StringName = N.ToString();
			*this << StringName;
		}
		return *this;
	}

	virtual FArchive& operator<<(class UObject*& Res) override
	{
		// Not supported through this archive
		assert(0);
		return *this;
	}

protected:
	FMemoryArchive()
		:FArchive(), Offset(0)
	{

	}
	int64 Offset;
};