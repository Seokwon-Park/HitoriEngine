#include "EnginePCH.h"
#include "Shader.h"
#include "Render/Renderer.h"

namespace
{
	static DXGI_FORMAT ToFormat(BYTE Mask, D3D_REGISTER_COMPONENT_TYPE Type)
	{
		static const DXGI_FORMAT Table[4][3] =
		{   // UINT32                          SINT32                          FLOAT32
			{ DXGI_FORMAT_R32_UINT,            DXGI_FORMAT_R32_SINT,            DXGI_FORMAT_R32_FLOAT },
			{ DXGI_FORMAT_R32G32_UINT,         DXGI_FORMAT_R32G32_SINT,         DXGI_FORMAT_R32G32_FLOAT },
			{ DXGI_FORMAT_R32G32B32_UINT,      DXGI_FORMAT_R32G32B32_SINT,      DXGI_FORMAT_R32G32B32_FLOAT },
			{ DXGI_FORMAT_R32G32B32A32_UINT,   DXGI_FORMAT_R32G32B32A32_SINT,   DXGI_FORMAT_R32G32B32A32_FLOAT },
		};

		const uint32 Count = (Mask & 0x8) ? 4 : (Mask & 0x4) ? 3 : (Mask & 0x2) ? 2 : (Mask & 0x1) ? 1 : 0;
		if (Count == 0) return DXGI_FORMAT_UNKNOWN;

		switch (Type)
		{
		case D3D_REGISTER_COMPONENT_UINT32:  return Table[Count - 1][0];
		case D3D_REGISTER_COMPONENT_SINT32:  return Table[Count - 1][1];
		case D3D_REGISTER_COMPONENT_FLOAT32: return Table[Count - 1][2];
		default:                             return DXGI_FORMAT_UNKNOWN;
		}
	}
}

FShaderProgram::FShaderProgram(FVertexShader* InVertexShader, FPixelShader* InPixelShader)
	:VertexShader(InVertexShader), PixelShader(InPixelShader)
{
}

const FConstantBufferBinding* FShaderBindings::FindConstantBuffer(const FString& Name) const
{
	for (auto& Binding : ConstantBuffers)
	{
		if (Binding.Name == Name)
			return &Binding;
	}
	return nullptr;
}

const FTextureBinding* FShaderBindings::FindTexture(const FString& Name) const
{
	for (auto& Binding : Textures)
	{
		if (Binding.Name == Name)
			return &Binding;
	}
	return nullptr;
}

const FSamplerBinding* FShaderBindings::FindSampler(const FString& Name) const
{
	for (auto& Binding : Samplers)
	{
		if (Binding.Name == Name)
			return &Binding;
	}
	return nullptr;
}

void FShader::BindingReflection(const FShaderByteCode& ByteCode)
{
	ComPtr<ID3D11ShaderReflection> Reflection;
	if (FAILED(D3DReflect(ByteCode.GetData(), ByteCode.GetSize(), IID_PPV_ARGS(Reflection.GetAddressOf())))) return;

	D3D11_SHADER_DESC Desc{};
	Reflection->GetDesc(&Desc);

	for (uint32 i = 0; i < Desc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC Bind{};
		Reflection->GetResourceBindingDesc(i, &Bind);

		switch (Bind.Type)
		{
		case D3D_SIT_CBUFFER:
		{
			D3D11_SHADER_BUFFER_DESC CbDesc{};
			Reflection->GetConstantBufferByName(Bind.Name)->GetDesc(&CbDesc);
			Bindings.ConstantBuffers.Add({ Bind.Name, Bind.BindPoint, CbDesc.Size });
			break;
		}
		case D3D_SIT_TEXTURE:
		{
			Bindings.Textures.Add({ Bind.Name, Bind.BindPoint, Bind.Dimension });
			break;
		}
		case D3D_SIT_SAMPLER:
		{
			Bindings.Samplers.Add({ Bind.Name, Bind.BindPoint });
			break;
		}
		default:
			break;
		}
	}
}

FVertexShader::FVertexShader(ID3D11Device* Device, const FShaderByteCode& ByteCode)
{
	Type = EShaderType::Vertex;
	if (!ByteCode.IsValid()) return;

	if (FAILED(Device->CreateVertexShader(ByteCode.GetData(), ByteCode.GetSize(), nullptr, VertexShader.GetAddressOf())))
	{
		return;
	}

	BindingReflection(ByteCode);
	InputLayoutReflection(Device, ByteCode);
}

void FVertexShader::InputLayoutReflection(ID3D11Device* Device, const FShaderByteCode& ByteCode)
{
	ComPtr<ID3D11ShaderReflection> Reflection;
	if (FAILED(D3DReflect(ByteCode.GetData(), ByteCode.GetSize(), IID_PPV_ARGS(Reflection.GetAddressOf())))) return;

	D3D11_SHADER_DESC Desc{};
	Reflection->GetDesc(&Desc);


	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayoutDesc;
	for (uint32 i = 0; i < Desc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
		Reflection->GetInputParameterDesc(i, &ParamDesc);

		if (ParamDesc.SystemValueType != D3D_NAME_UNDEFINED) continue;

		const DXGI_FORMAT Format = ToFormat(ParamDesc.Mask, ParamDesc.ComponentType);
		if (Format == DXGI_FORMAT_UNKNOWN) continue;

		D3D11_INPUT_ELEMENT_DESC ElementDesc;
		ElementDesc.SemanticName = ParamDesc.SemanticName;
		ElementDesc.SemanticIndex = ParamDesc.SemanticIndex;
		ElementDesc.Format = Format;
		ElementDesc.InputSlot = 0;
		ElementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		ElementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		ElementDesc.InstanceDataStepRate = 0;

		InputLayoutDesc.Add(ElementDesc);
	}

	if (InputLayoutDesc.Num() == 0) return;

	HRESULT hr = Device->CreateInputLayout(InputLayoutDesc.GetData(), InputLayoutDesc.Num(), ByteCode.GetData(), ByteCode.GetSize(), InputLayout.GetAddressOf());

	if (FAILED(hr))
		HTR_LOG(Error, "[Shader] CreateInputLayout Failed!");

}

FPixelShader::FPixelShader(ID3D11Device* Device, const FShaderByteCode& ByteCode)
{
	Type = EShaderType::Pixel;
	if (!ByteCode.IsValid()) return;

	if (FAILED(Device->CreatePixelShader(ByteCode.GetData(), ByteCode.GetSize(), nullptr, PixelShader.GetAddressOf())))
	{
		return;
	}

	BindingReflection(ByteCode);
}

FComputeShader::FComputeShader(ID3D11Device* Device, const FShaderByteCode& ByteCode)
{
	Type = EShaderType::Compute;
	if (!ByteCode.IsValid())return;

	if (FAILED(Device->CreateComputeShader(ByteCode.GetData(), ByteCode.GetSize(), nullptr, ComputeShader.GetAddressOf())))
	{
		return;
	}
}
