#pragma once

#include "Archive.h"

struct FLazyObjectPtr;
struct FObjectPtr;
struct FSoftObjectPath;
struct FSoftObjectPtr;
struct FWeakObjectPtr;

class FArchiveUObject : public FArchive
{
public:

	//static FArchive& SerializeLazyObjectPtr(FArchive& Ar, FLazyObjectPtr& Value);
	//static FArchive& SerializeObjectPtr(FArchive& Ar, FObjectPtr& Value);
	//static FArchive& SerializeSoftObjectPtr(FArchive& Ar, FSoftObjectPtr& Value);
	//static FArchive& SerializeSoftObjectPath(FArchive& Ar, FSoftObjectPath& Value);
	//static FArchive& SerializeWeakObjectPtr(FArchive& Ar, FWeakObjectPtr& Value);

	using FArchive::operator<<;

	//virtual FArchive& operator<<(struct FLazyObjectPtr& Value) override { return SerializeLazyObjectPtr(*this, Value); }
	//virtual FArchive& operator<<(struct FObjectPtr& Value) override { return SerializeObjectPtr(*this, Value); }
	//virtual FArchive& operator<<(struct FSoftObjectPtr& Value) override { return SerializeSoftObjectPtr(*this, Value); }
	//virtual FArchive& operator<<(struct FSoftObjectPath& Value) override { return SerializeSoftObjectPath(*this, Value); }
	//virtual FArchive& operator<<(struct FWeakObjectPtr& Value) override { return SerializeWeakObjectPtr(*this, Value); }
};