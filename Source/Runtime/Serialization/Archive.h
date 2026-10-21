#pragma once

#include "Core/Types.h"   // DEFINE_ENUM_OPERATORS

class UObject;
class FName;

enum class EPropertyPortFlags : uint32
{
	PPF_None = 0,
	PPF_Duplicate = 1 << 0,   // 객체 복제 중
	PPF_DuplicateForPIE = 1 << 1,   // PIE 월드 복제 중
};
DEFINE_ENUM_OPERATORS(EPropertyPortFlags)

struct FArchiveState
{
public:
	virtual ~FArchiveState() = default;

	bool IsLoading() const { return ArIsLoading; }
	bool IsSaving() const { return ArIsSaving; }
	bool IsPersistent() const { return ArIsPersistent; }
	bool IsTextFormat() const { return ArIsTextFormat; }
	bool IsError() const { return ArIsError; }

	EPropertyPortFlags GetPortFlags() const { return ArPortFlags; }
	bool HasAnyPortFlags(EPropertyPortFlags Flags) const { return HasFlag(ArPortFlags, Flags); }
	bool HasAllPortFlags(EPropertyPortFlags Flags) const { return (ArPortFlags & Flags) == Flags; }
	void SetPortFlags(EPropertyPortFlags InPortFlags) { ArPortFlags = InPortFlags; }

	bool GetError() const { return ArIsError; }
	void SetError();
	void ClearError() { ArIsError = false; }

	virtual void SetIsLoading(bool bInIsLoading);
	virtual void SetIsSaving(bool bInIsSaving);
	virtual void SetIsTextFormat(bool bInIsTextFormat);
	virtual void SetIsPersistent(bool bInIsPersistent);

	void Reset() { *this = FArchiveState(); }
protected:
	bool ArIsLoading : 1 = 0;
	bool ArIsSaving : 1 = 0;
	bool ArIsPersistent : 1 = 0;
	bool ArIsTextFormat : 1 = 0;
private:
	bool ArIsError : 1 = 0;
	EPropertyPortFlags ArPortFlags = EPropertyPortFlags::PPF_None;
};

class FArchive : public FArchiveState
{
public:
	FArchive() = default;
	FArchive(const FArchive&) = default;
	FArchive& operator =(const FArchive& ArchiveToCopy) = default;
	virtual ~FArchive() = default;

	virtual void Serialize(void* V, int64 Length) {};
	virtual void SerializeBool(bool& D);
	virtual int64 Tell() { return -1; }
	virtual void Seek(int64 InPos) {};

	virtual int64 TotalSize() { return -1; }

	virtual FArchive& operator<<(UObject*& Value) { return *this; };
	//virtual FArchive& operator<<(struct FLazyObjectPtr& Value) { return *this; }
	//virtual FArchive& operator<<(struct FObjectPtr& Value) { return *this; }
	//virtual FArchive& operator<<(struct FSoftObjectPtr& Value) { return *this; }
	//virtual FArchive& operator<<(struct FSoftObjectPath& Value) { return *this; }
	//virtual FArchive& operator<<(struct FWeakObjectPtr& Value) { return *this; }
	virtual FArchive& operator<<(FName& Value) { return *this; }

	friend FArchive& operator<<(FArchive& Ar, int8& Value) { Ar.Serialize(&Value, 1); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, uint8& Value) { Ar.Serialize(&Value, 1); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, int16& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, uint16& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, int32& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, uint32& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, int64& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, uint64& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, float& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, double& Value) { Ar.Serialize(&Value, sizeof(Value)); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, bool& Value) { Ar.SerializeBool(Value); return Ar; }
	friend FArchive& operator<<(FArchive& Ar, FString& Value);

protected:
private:

};