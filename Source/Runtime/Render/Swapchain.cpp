#include "EnginePCH.h"
#include "Swapchain.h"
#include "Texture2D.h"

#include "Core/Window.h"

#include <dxgi1_5.h>

namespace
{
	// 보더리스 창은 DWM이 합성하므로 SyncInterval 0만으로는 주사율에 묶인다.
	// Tearing을 허용하면 합성기를 기다리지 않고 Present할 수 있다.
	bool QueryTearingSupport(IDXGIFactory* Factory)
	{
		ComPtr<IDXGIFactory5> Factory5;
		if (!Factory || FAILED(Factory->QueryInterface(IID_PPV_ARGS(Factory5.GetAddressOf()))))
			return false;

		BOOL bSupported = FALSE;
		if (FAILED(Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bSupported, sizeof(bSupported))))
			return false;

		return bSupported == TRUE;
	}
}

FSwapchain::FSwapchain(FRenderDevice* InRenderDevice, FWindow* InWindow)
{
	RenderDevice = InRenderDevice;

	DXGI_SAMPLE_DESC SampleDesc{};
	SampleDesc.Count = 1;
	SampleDesc.Quality = 0;

	DXGI_MODE_DESC BufferDesc{};
	BufferDesc.Width = InWindow->GetWidth();
	BufferDesc.Height = InWindow->GetHeight();
	BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	Desc.BufferDesc = BufferDesc;
	Desc.SampleDesc = SampleDesc;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = 2;
	Desc.OutputWindow = InWindow->GetHandle();
	Desc.Windowed = true;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// ALLOW_TEARING은 전체화면 전환과 함께 쓸 수 없으므로 둘 중 하나만 지정한다.
	bAllowTearing = QueryTearingSupport(RenderDevice->GetFactory());
	Desc.Flags = bAllowTearing
		? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
		: DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hr = RenderDevice->GetFactory()->CreateSwapChain(RenderDevice->GetDevice(), &Desc, Swapchain.GetAddressOf());

	// 플래그 조합을 거부하는 드라이버가 있으므로 Tearing 없이 한 번 더 시도한다.
	if (FAILED(hr) && bAllowTearing)
	{
		HTR_LOG(Warning, "Swapchain creation with ALLOW_TEARING failed. Retrying without it.");
		bAllowTearing = false;
		Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		hr = RenderDevice->GetFactory()->CreateSwapChain(RenderDevice->GetDevice(), &Desc, Swapchain.GetAddressOf());
	}

	if (FAILED(hr))
		HTR_LOG(Error, "Failed To Create Swapchain!");
	else
		HTR_LOG(Info, "Swapchain created. Tearing {}", bAllowTearing ? "allowed" : "not allowed");

	CreateBackbuffer();
}

void FSwapchain::CreateBackbuffer()
{
	ComPtr<ID3D11Texture2D> Backbuffer;
	Swapchain->GetBuffer(
		0,
		IID_PPV_ARGS(&Backbuffer)
	);

	D3D11_TEXTURE2D_DESC TextureDesc;
	Backbuffer->GetDesc(&TextureDesc);

	BackbufferTexture = MakeUnique<FTexture2D>(RenderDevice->GetDevice(), Backbuffer, TextureDesc);
}

FSwapchain::~FSwapchain()
{
}

void FSwapchain::Resize(int32 InWidth, int32 InHeight)
{
	BackbufferTexture = nullptr;

	// 생성 시 플래그를 그대로 넘기지 않으면 ALLOW_TEARING이 풀린다.
	Swapchain->ResizeBuffers(
		0,
		InWidth,
		InHeight,
		DXGI_FORMAT_UNKNOWN,
		Desc.Flags
	);

	//Update Desc
	Swapchain->GetDesc(&Desc);

	CreateBackbuffer();
}

void FSwapchain::SwapBuffers(uint32 SyncInterval, uint32 Flags)
{
	// ALLOW_TEARING은 SyncInterval이 0일 때만 유효하다. 1과 함께 넘기면 Present가 실패한다.
	if (bAllowTearing && SyncInterval == 0)
		Flags |= DXGI_PRESENT_ALLOW_TEARING;

	Swapchain->Present(SyncInterval, Flags);
}

