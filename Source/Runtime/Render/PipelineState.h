#pragma once

#include "Shader.h"
#include "RenderStates.h"


struct FPipelineState
{
	FPipelineState();
	~FPipelineState() = default;

	FShaderProgram* Shader = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ERasterizerState         RasterizerState = ERasterizerState::SolidBack;
	EBlendState              BlendState = EBlendState::Opaque;
	EDepthStencilState       DepthStencilState = EDepthStencilState::Default;
};