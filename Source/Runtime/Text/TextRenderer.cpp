#include "EnginePCH.h"
#include "TextRenderer.h"

#include "Camera/CameraComponent.h"

#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Vertex.h"

#include "Font.h"

namespace
{
	TArray<uint32> DecodeUTF8(const FString& Str)
	{
		TArray<uint32> Out;
		const unsigned char* P = reinterpret_cast<const unsigned char*>(Str.c_str());
		const unsigned char* End = P + Str.size();

		while (P < End)
		{
			uint32 CP = 0;
			int Len = 0;
			if (*P < 0x80) { CP = *P;        Len = 1; }
			else if ((*P & 0xE0) == 0xC0) { CP = *P & 0x1F; Len = 2; }
			else if ((*P & 0xF0) == 0xE0) { CP = *P & 0x0F; Len = 3; }
			else if ((*P & 0xF8) == 0xF0) { CP = *P & 0x07; Len = 4; }
			else { ++P; continue; }                         // 잘못된 바이트 스킵

			if (P + Len > End) break;
			for (int i = 1; i < Len; ++i) CP = (CP << 6) | (P[i] & 0x3F);

			Out.Add(CP);
			P += Len;
		}
		return Out;
	}
}

// 텍스트 Shader·상수 버퍼와 문자용 정점·인덱스 버퍼를 준비한다.
void FTextRenderer::Init(uint32 InMaxCharacters)
{
	MaxCharacters = InMaxCharacters;
	MaxVertices = MaxCharacters * 4; // 글자당 쿼드 4버텍스
	MaxIndices = MaxCharacters * 6;  // 글자당 삼각형 2개(6인덱스)

	// 다이나믹 버텍스/인덱스 버퍼 생성 (D3D11_USAGE_DYNAMIC, CPU_ACCESS_WRITE)
	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FTextVertex) * MaxVertices, sizeof(FTextVertex));
	IndexBuffer = RenderCommand::CreateDynamicIndexBuffer(sizeof(uint32) * MaxIndices);

	MVP = RenderCommand::CreateConstantBuffer(sizeof(TextTransformData));
	ScreenPx = RenderCommand::CreateConstantBuffer(sizeof(MSDFData));

	TextShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/TextShader.hlsl");

	PipelineState.Shader = TextShader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

// Atlas 글리프 위치·UV로 문자별 사각형 Mesh를 만든다.
void FTextRenderer::BuildTextMesh(const FString& Text, float TextSize, const UFont& Atlas)
{
	TArray<uint32> CodePoints = DecodeUTF8(Text);

	float PenY = 0.0f;

	// 전체 텍스트 너비를 미리 계산해서 중앙 정렬 (UUID 라벨은 보통 중앙 정렬이 자연스러움)
	float TotalWidth = 0.0f;

	for (uint32 CP : CodePoints)
	{
		if (const FGlyphInfo* Glyph = Atlas.GlyphMap.Find(CP))
		{
			TotalWidth += Glyph->Advance * TextSize;
		}
	}
	float StartY = -TotalWidth * 0.5f;
	PenY = StartY;

	for (uint32 CP : CodePoints)          // 메시 생성 루프
	{
		const FGlyphInfo* Glyph = Atlas.GlyphMap.Find(CP);
		if (!Glyph)
		{
			continue;
		}

		if (Glyph->bHasBounds)
		{
			float Left = PenY + Glyph->PlaneLeft * TextSize;
			float Bottom = Glyph->PlaneBottom * TextSize;
			float Right = PenY + Glyph->PlaneRight * TextSize;
			float Top = Glyph->PlaneTop * TextSize;

			float ULeft = Glyph->AtlasLeft / Atlas.Width;
			float VBottom = 1.0f - (Glyph->AtlasBottom / Atlas.Height);
			float URight = Glyph->AtlasRight / Atlas.Width;
			float VTop = 1.0f - (Glyph->AtlasTop / Atlas.Height);

			uint32 BaseIndex = static_cast<uint32>(Vertices.Num());

			Vertices.Add({ FVector(0.0f,Left, Bottom), FVector2(ULeft, VBottom) });
			Vertices.Add({ FVector(0.0f,Left, Top), FVector2(ULeft, VTop) });
			Vertices.Add({ FVector(0.0f, Right,  Top), FVector2(URight, VTop) });
			Vertices.Add({ FVector(0.0f, Right, Bottom), FVector2(URight,VBottom) });

			Indices.Add(BaseIndex + 0);
			Indices.Add(BaseIndex + 1);
			Indices.Add(BaseIndex + 2);
			Indices.Add(BaseIndex + 0);
			Indices.Add(BaseIndex + 2);
			Indices.Add(BaseIndex + 3);
		}

		PenY += Glyph->Advance * TextSize;
	}
}

bool FTextRenderer::ComputeTextBounds(const FString& Text, float TextSize, const UFont& Atlas, float& OutMinY, float& OutMaxY, float& OutMinZ, float& OutMaxZ)
{
	TArray<uint32> CodePoints = DecodeUTF8(Text);

	float PenY = 0.0f;

	// 전체 텍스트 너비를 미리 계산해서 중앙 정렬 (UUID 라벨은 보통 중앙 정렬이 자연스러움)
	float TotalWidth = 0.0f;
	for (uint32 CP : CodePoints)
	{
		if (const FGlyphInfo* Glyph = Atlas.GlyphMap.Find(CP))
		{
			TotalWidth += Glyph->Advance * TextSize;
		}
	}
	float StartY = -TotalWidth * 0.5f;
	PenY = StartY;

	float Left = FLT_MAX;
	float Bottom = FLT_MAX;
	float Right = -FLT_MAX;
	float Top = -FLT_MAX;

	for (uint32 CP : CodePoints)          // 메시 생성 루프
	{
		const FGlyphInfo* Glyph = Atlas.GlyphMap.Find(CP);
		if (!Glyph)
		{
			continue;
		}

		if (Glyph->bHasBounds)
		{
			Left = std::min(Left, PenY + Glyph->PlaneLeft * TextSize);
			Bottom = std::min(Bottom, Glyph->PlaneBottom * TextSize);
			Right = std::max(Right, PenY + Glyph->PlaneRight * TextSize);
			Top = std::max(Top, Glyph->PlaneTop * TextSize);
		}

		PenY += Glyph->Advance * TextSize;
	}


	if (Left > Right)   // 한 번도 갱신 안 됨 = 그릴 글자 없음
		return false;

	OutMinY = Left;
	OutMaxY = Right;
	OutMinZ = Bottom;
	OutMaxZ = Top;

	return true;
}

// 카메라 ViewProjection을 명시적 행렬 렌더 경로로 전달한다.
void FTextRenderer::OnRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
	const UFont& Atlas, UCameraComponent* CameraComponent)
{
	OnRender(Text, WorldMatrix, TextSize, Atlas, CameraComponent->GetViewProjectionMatrix());
}

// ViewProjection과 MSDF 상수로 텍스트 Mesh를 그린다.
void FTextRenderer::OnRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
	const UFont& Atlas, const FMatrix& ViewProjection)
{
	if (Text.empty() || !Atlas.AtlasTexture)
	{
		return;
	}

	// 이 텍스트의 메시를 로컬 좌표로 새로 만든다
	Vertices.Reset();
	Indices.Reset();
	BuildTextMesh(Text, TextSize, Atlas);

	if (Vertices.IsEmpty())
	{
		return;
	}

	// 버퍼 용량 초과 체크
	if (Vertices.Num() > (int32)MaxVertices || Indices.Num() > (int32)MaxIndices)
	{
		HTR_LOG(Warning, "Text too long for text buffer ({} chars max): {}", MaxCharacters, Text);
		return;
	}

	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FTextVertex) * Vertices.Num());
	RenderCommand::UpdateBufferData(IndexBuffer.get(), Indices.GetData(), sizeof(uint32) * Indices.Num());

	TextTransformData TransData;
	TransData.World = WorldMatrix;
	TransData.ViewProj = ViewProjection;

	MSDFData MSDFData;
	MSDFData.ScreenPx = Atlas.DistanceRange;

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindShaderResource(0, Atlas.AtlasTexture, EShaderBindFlagBits::Pixel);
	RenderCommand::UpdateBufferData(MVP.get(), &TransData, sizeof(TextTransformData));
	RenderCommand::UpdateBufferData(ScreenPx.get(), &MSDFData, sizeof(MSDFData));

	RenderCommand::BindConstantBuffer(0, MVP.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(1, ScreenPx.get(), EShaderBindFlagBits::Pixel);

	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindIndexBuffer(IndexBuffer.get());
	RenderCommand::DrawIndexed(Indices.Num());

	Vertices.Reset();
	Indices.Reset();
}

// 카메라를 향하는 Billboard 행렬을 구성해 텍스트를 그린다.
void FTextRenderer::OnRenderBillboard(const FString& Text, const FVector& WorldPos, float TextSize,
	const UFont& Atlas, UCameraComponent* CameraComponent)
{
	const FTransform& Transform = CameraComponent->GetTransform();

	const FVector Forward = Transform.GetForward().Normalized();
	const FVector Right = Transform.GetRight().Normalized();
	const FVector Up = Transform.GetUp().Normalized();

	// 텍스트 로컬축: X = , Y = 글자 가로, Z = 글자 세로
	FMatrix World;
	World.SetIdentity();
	World.M[0][0] = Forward.X;  World.M[0][1] = Forward.Y;  World.M[0][2] = Forward.Z;  World.M[0][3] = 0;
	World.M[1][0] = Right.X;    World.M[1][1] = Right.Y;    World.M[1][2] = Right.Z;    World.M[1][3] = 0;
	World.M[2][0] = Up.X;       World.M[2][1] = Up.Y;       World.M[2][2] = Up.Z;       World.M[2][3] = 0;
	World.M[3][0] = WorldPos.X; World.M[3][1] = WorldPos.Y; World.M[3][2] = WorldPos.Z; World.M[3][3] = 1;

	OnRender(Text, World, TextSize, Atlas, CameraComponent);
}
