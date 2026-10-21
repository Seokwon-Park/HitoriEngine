#pragma once

struct FImageData
{
	FImageData() = default;

	bool IsValid() const { return !Pixels.IsEmpty(); }

	// 행 하나의 바이트 수. FTexture2D가 SysMemPitch로 쓴다.
	// BC 압축 포맷은 4x4 블록 한 줄이 한 행이라 Width * BytesPerPixel로 구할 수 없으므로 로더가 RowPitch를 직접 채운다.
	uint32 GetRowPitch() const { return RowPitch != 0 ? RowPitch : Width * BytesPerPixel; }

	TArray<uint8> Pixels;
	TArray<uint32> MipOffsets;
	TArray<uint32> MipRowPitches;
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 BytesPerPixel = 0;   // BC 압축 포맷이면 0
	uint32 RowPitch = 0;        // 0이면 Width * BytesPerPixel
	uint32 MipLevels = 1;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class ImageLoader
{
public:
	static FImageData Load(const FString& Path);
	static FImageData LoadHDR(const FString& Path);
	static FImageData LoadDDS(const FString& Path);
	static FImageData LoadAuto(const FString& Path);
};
