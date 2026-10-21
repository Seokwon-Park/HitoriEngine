#pragma once

class FLargeMemoryData
{
public:
	explicit FLargeMemoryData(const int64 PreAllocateBytes = 0);
	~FLargeMemoryData();

	bool Write(void* InData, int64 InOffset, int64 InNum);
	void Append(void* InData, int64 InNum)
	{
		Write(InData, NumBytes, InNum);
	}

	bool Read(void* OutData, int64 InOffset, int64 InNum) const;

	int64 GetSize() const
	{
		return NumBytes;
	}

	uint8* GetData()
	{
		return Data;
	}

	const uint8* GetData() const
	{
		return Data;
	}

	uint8* ReleaseOwnership();

	bool HasData() const
	{
		return Data != nullptr;
	}

	void Reserve(int64 Size);
private:

	// 복사 절대 금지
	FLargeMemoryData(const FLargeMemoryData&) = delete;
	FLargeMemoryData& operator=(const FLargeMemoryData&) = delete;

	// Archive에 의해 관리되는 데이터
	uint8* Data;

	// 현재 쓰인 바이트
	int64 NumBytes;

	// 전체 바이트
	int64 MaxBytes;

	// 크기 재조정
	void GrowBuffer();
};