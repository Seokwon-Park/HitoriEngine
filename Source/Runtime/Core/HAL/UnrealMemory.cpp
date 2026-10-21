#include "EnginePCH.h"
#include "UnrealMemory.h"

uint64 FMemory::QuantizeSize(uint64 Count, uint32 Alignment)
{
	return (Count + Alignment - 1) & ~static_cast<uint64>(Alignment - 1);
}

void* FMemory::Malloc(uint64 Count)
{
	return std::malloc(static_cast<size_t>(Count));
}

void* FMemory::Realloc(void* Original, uint64 Count)
{
	return std::realloc(Original, static_cast<size_t>(Count));
}

void FMemory::Free(void* Original)
{
	std::free(Original);
}
