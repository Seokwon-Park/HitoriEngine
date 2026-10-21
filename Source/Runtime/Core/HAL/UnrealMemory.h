// Core/Memory.h (위치는 프로젝트 규칙에 맞게)
#pragma once

#include <cstring>
#include "Core/Types.h"

// UE의 FMemory 축소판. 지금은 표준 함수를 감싸기만 한다.
// 나중에 할당 추적이나 정렬 할당이 필요하면 이 층에서 바꾼다.
struct FMemory
{
	static void* Memcpy(void* Dest, const void* Src, uint64 Count) { return std::memcpy(Dest, Src, static_cast<size_t>(Count)); }
	static void* Memmove(void* Dest, const void* Src, uint64 Count) { return std::memmove(Dest, Src, static_cast<size_t>(Count)); }
	static int32 Memcmp(const void* A, const void* B, uint64 Count) { return std::memcmp(A, B, static_cast<size_t>(Count)); }
	static void* Memset(void* Dest, uint8 Value, uint64 Count) { return std::memset(Dest, Value, static_cast<size_t>(Count)); }
	static void* Memzero(void* Dest, uint64 Count) { return std::memset(Dest, 0, static_cast<size_t>(Count)); }

	static uint64 QuantizeSize(uint64 Count, uint32 Alignment = 16);

	static void* Malloc(uint64 Count);
	static void* Realloc(void* Original, uint64 Count);
	static void Free(void* Original);
};