#pragma once

#include "Core/Types.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"

// Forward와 Deferred가 공유하는 뷰포트의 색상, GBuffer, 깊이 리소스를 관리한다.
struct FSceneViewTargets
{
	TUniquePtr<FTexture2D> SceneColor;
	TUniquePtr<FTexture2D> Depth;

	// FXAA처리가 된 SceneColor로 swap하기 위해 사용된다.
	FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output)
	{
		const bool bCompatible = Output && 
								Output->GetWidth() == SceneColor->GetWidth() && Output->GetHeight() == SceneColor->GetHeight() &&
								Output->GetFormat() == SceneColor->GetFormat();
		if (!bCompatible)
			return nullptr;
		SceneColor.swap(Output);
		RenderCommand::BindRenderPassNoClear(SceneColor.get(), Depth.get(), SceneColor->GetWidth(), SceneColor->GetHeight());
		return SceneColor.get();
	}

	void Reset()
	{
		SceneColor.reset();
		Depth.reset();
	}

	bool IsValidFor(uint32 Width, uint32 Height) const
	{
		const auto HasSize = [Width, Height](const TUniquePtr<FTexture2D>& Target)
		{
			return Target && Target->GetRawPtr() &&
				Target->GetWidth() == Width && Target->GetHeight() == Height;
		};

		return HasSize(SceneColor) && SceneColor->GetRTV() && SceneColor->GetSRV() &&
			HasSize(Depth) && Depth->GetDSV() && Depth->GetSRV();
	}

	bool Resize(uint32 Width, uint32 Height)
	{
		if (IsValidFor(Width, Height))
			return true;

		Reset();
		if (Width == 0 || Height == 0)
			return false;

		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SceneColor = RenderCommand::CreateTexture2D(Desc);

		// DSV로 쓰고 SRV로 읽을 수 있도록 typeless 리소스로 만든다.
		Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		Depth = RenderCommand::CreateTexture2D(Desc);

		if (!IsValidFor(Width, Height))
		{
			Reset();
			return false;
		}
		return true;
	}
};
