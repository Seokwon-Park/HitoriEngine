#include "EnginePCH.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/Vertex.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "Editor/Settings/SettingsPanel.h"
#include "Render/RenderResourceManager.h"

namespace {
// 역 VP로 복원한 절두체 꼭짓점과 월드 축별 최대 가시 범위를 담는다.
struct FGridFrustum {
  FVector Corners[8];
  FVector AxisExtent;
};

// 작은 행렬식을 특이행렬로 오판하지 않도록 double 부분 피벗 소거로 Grid 전용
// 역행렬을 구한다.
bool TryInvertGridViewProjection(const FMatrix &Matrix, FMatrix &Inverse) {
  double Augmented[4][8]{};
  for (int Row = 0; Row < 4; ++Row) {
    for (int Column = 0; Column < 4; ++Column) {
      if (!std::isfinite(Matrix.M[Row][Column]))
        return false;
      Augmented[Row][Column] = Matrix.M[Row][Column];
    }
    Augmented[Row][Row + 4] = 1.0;
  }
  for (int Column = 0; Column < 4; ++Column) {
    int PivotRow = Column;
    for (int Row = Column + 1; Row < 4; ++Row)
      if (std::fabs(Augmented[Row][Column]) >
          std::fabs(Augmented[PivotRow][Column]))
        PivotRow = Row;
    const double Pivot = Augmented[PivotRow][Column];
    if (!std::isfinite(Pivot) || Pivot == 0.0)
      return false;
    for (int Entry = 0; Entry < 8; ++Entry)
      std::swap(Augmented[Column][Entry], Augmented[PivotRow][Entry]);
    for (int Entry = 0; Entry < 8; ++Entry)
      Augmented[Column][Entry] /= Pivot;
    for (int Row = 0; Row < 4; ++Row) {
      if (Row == Column)
        continue;
      const double Factor = Augmented[Row][Column];
      for (int Entry = 0; Entry < 8; ++Entry)
        Augmented[Row][Entry] -= Factor * Augmented[Column][Entry];
    }
  }
  for (int Row = 0; Row < 4; ++Row)
    for (int Column = 0; Column < 4; ++Column) {
      const double Value = Augmented[Row][Column + 4];
      if (!std::isfinite(Value) ||
          std::fabs(Value) > std::numeric_limits<float>::max())
        return false;
      Inverse.M[Row][Column] = static_cast<float>(Value);
    }
  return true;
}

// D3D 깊이 0~1의 여덟 모서리를 역투영해 고정 길이가 아닌 실제 가시 범위를
// 구한다.
bool BuildGridFrustum(const FMatrix &ViewProj, FGridFrustum &Result) {
  FMatrix Inverse;
  if (!TryInvertGridViewProjection(ViewProj, Inverse))
    return false;
  Result.AxisExtent = FVector(1, 1, 1);
  for (int Index = 0; Index < 8; ++Index) {
    const FVector4 H =
        FVector4(Index & 1 ? 1.0f : -1.0f, Index & 2 ? 1.0f : -1.0f,
                 Index & 4 ? 1.0f : 0.0f, 1.0f) *
        Inverse;
    if (!std::isfinite(H.W) || std::fabs(H.W) < 1.0e-12f)
      return false;
    const FVector P(H.X / H.W, H.Y / H.W, H.Z / H.W);
    if (!std::isfinite(P.X) || !std::isfinite(P.Y) || !std::isfinite(P.Z))
      return false;
    Result.Corners[Index] = P;
    Result.AxisExtent.X = std::max(Result.AxisExtent.X, std::fabs(P.X) + 1.0f);
    Result.AxisExtent.Y = std::max(Result.AxisExtent.Y, std::fabs(P.Y) + 1.0f);
    Result.AxisExtent.Z = std::max(Result.AxisExtent.Z, std::fabs(P.Z) + 1.0f);
  }
  return true;
}

// 절두체의 12개 모서리와 월드 평면의 교점을 모아 화면을 덮는 Grid 범위를
// 구한다.
bool GetVisibleGridBounds(const FGridFrustum &Frustum, EGridPlane Plane,
                          float &MinU, float &MaxU, float &MinV, float &MaxV) {
  MinU = MinV = std::numeric_limits<float>::max();
  MaxU = MaxV = -std::numeric_limits<float>::max();
  // 선택한 평면의 법선 좌표를 부호 있는 거리로 사용한다.
  const auto Distance = [Plane](const FVector &P) {
    return Plane == EGridPlane::XY ? P.Z : Plane == EGridPlane::XZ ? P.Y : P.X;
  };
  // 교점을 해당 평면의 두 좌표로 치환해 최소·최대를 누적한다.
  const auto Include = [&](const FVector &P) {
    const float U = Plane == EGridPlane::YZ ? P.Y : P.X;
    const float V = Plane == EGridPlane::XY ? P.Y : P.Z;
    MinU = std::min(MinU, U);
    MaxU = std::max(MaxU, U);
    MinV = std::min(MinV, V);
    MaxV = std::max(MaxV, V);
  };
  for (int Index = 0; Index < 8; ++Index) {
    for (int Bit : {1, 2, 4}) {
      if (Index & Bit)
        continue;
      const FVector A = Frustum.Corners[Index],
                    B = Frustum.Corners[Index | Bit];
      const float DA = Distance(A), DB = Distance(B);
      if (std::fabs(DA) < 1.0e-5f)
        Include(A);
      if (std::fabs(DB) < 1.0e-5f)
        Include(B);
      if ((DA < 0) != (DB < 0))
        Include(A + (B - A) * (DA / (DA - DB)));
    }
  }
  return MinU <= MaxU && MinV <= MaxV;
}
} // namespace

// 재사용하던 선 정점 배열을 해제한다.
FGridRenderer::~FGridRenderer() { delete[] BatchGridVertices; }

// 원근 Grid와 화면 두께 기반 선의 Shader·재사용 버퍼·깊이 상태를 준비한다.
bool FGridRenderer::Init(FRenderer *InRenderer) {
  Renderer = InRenderer;
  const std::vector<D3D11_INPUT_ELEMENT_DESC> Layout = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       offsetof(FGridLineVertex, Start), D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       offsetof(FGridLineVertex, End), D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       offsetof(FGridLineVertex, Shape), D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       offsetof(FGridLineVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0}};
  static_assert(sizeof(FBatchGridData) == 96,
                "Grid line constant buffer layout mismatch");
  BatchGridShader = FRenderResourceManager::GetShaderProgram(
      "Resources/Shader/GridShaderBatch.hlsl");
  MaxVertices = 100000;
  BatchGridVertices = new FGridLineVertex[MaxVertices];
  BatchGridVertexBuffer = RenderCommand::CreateDynamicVertexBuffer(
      sizeof(FGridLineVertex) * MaxVertices, sizeof(FGridLineVertex));
  BatchGridConstantBuffer =
      RenderCommand::CreateConstantBuffer(sizeof(FBatchGridData));

  PSGridShader = FRenderResourceManager::GetShaderProgram(
      "Resources/Shader/GridShader.hlsl");
  PSGridConstantBuffer =
      RenderCommand::CreateConstantBuffer(sizeof(FPSGridData));

  BatchGridPipelineState.Shader = BatchGridShader;
  BatchGridPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  BatchGridPipelineState.RasterizerState = ERasterizerState::SolidNone;
  BatchGridPipelineState.BlendState = EBlendState::AlphaBlend;
  BatchGridPipelineState.DepthStencilState = EDepthStencilState::ReadOnly;

  PSGridPipelineState.Shader = PSGridShader;
  PSGridPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
  PSGridPipelineState.RasterizerState = ERasterizerState::SolidNone;
  PSGridPipelineState.BlendState = EBlendState::AlphaBlend;
  PSGridPipelineState.DepthStencilState = EDepthStencilState::ReadOnly;
  return true;
}

// 양 끝점은 월드 좌표로 유지하고 삼각형 선택·두께만 기록해 GPU에서 VP를
// 적용하게 한다.
void FGridRenderer::AddWorldLine(const FVector &Start, const FVector &End,
                                 const FVector4 &Color, float HalfWidth,
                                 uint32 &VertexCount) {
  if (VertexCount + 6 > MaxVertices)
    return;
  const FGridLineVertex Corners[] = {
      {Start, End, FVector(0, 1, HalfWidth), Color},
      {Start, End, FVector(0, -1, HalfWidth), Color},
      {Start, End, FVector(1, 1, HalfWidth), Color},
      {Start, End, FVector(1, -1, HalfWidth), Color}};
  for (const int Index : {0, 1, 2, 2, 1, 3})
    BatchGridVertices[VertexCount++] = Corners[Index];
}

// 월드 축을 원점에서 나눠 같은 두께로 만들고 모든 축의 음수 방향을 옅게
// 표시한다.
void FGridRenderer::AddWorldAxes(EGridPlane Plane, bool bAllAxes,
                                 const FVector &AxisExtent,
                                 uint32 &VertexCount) {
  const float XExtent = AxisExtent.X;
  const float YExtent = AxisExtent.Y;
  const float ZExtent = AxisExtent.Z;
  if (bAllAxes || Plane != EGridPlane::YZ) {
    AddWorldLine(FVector(-XExtent, 0, 0), FVector(0, 0, 0),
                 FVector4(1, 0, 0, 0.3f), 1.5f, VertexCount);
    AddWorldLine(FVector(0, 0, 0), FVector(XExtent, 0, 0),
                 FVector4(1, 0, 0, 1.0f), 1.5f, VertexCount);
  }
  if (bAllAxes || Plane != EGridPlane::XZ) {
    AddWorldLine(FVector(0, -YExtent, 0), FVector(0, 0, 0),
                 FVector4(0, 1, 0, 0.3f), 1.5f, VertexCount);
    AddWorldLine(FVector(0, 0, 0), FVector(0, YExtent, 0),
                 FVector4(0, 1, 0, 1.0f), 1.5f, VertexCount);
  }
  if (bAllAxes || Plane != EGridPlane::XY) {
    AddWorldLine(FVector(0, 0, -ZExtent), FVector(0, 0, 0),
                 FVector4(0.25f, 0.45f, 0.90f, 0.3f), 1.5f, VertexCount);
    AddWorldLine(FVector(0, 0, 0), FVector(0, 0, ZExtent),
                 FVector4(0.25f, 0.45f, 0.90f, 1.0f), 1.5f, VertexCount);
  }
}

// X/Y Grid와 같은 행렬 업로드 규약으로 월드 선을 투영하고 깊이 검사로 가림을
// 유지한다.
void FGridRenderer::DrawWorldLines(uint32 VertexCount, const FMatrix &ViewProj,
                                   const FViewportSettings &Viewport,
                                   const FVector &FadeOrigin,
                                   float FadeRadius) {
  if (VertexCount == 0 || Viewport.Width == 0 || Viewport.Height == 0)
    return;
  FBatchGridData Data{};
  Data.ViewProjection = ViewProj;
  Data.ViewportSize = FVector2(static_cast<float>(Viewport.Width),
                               static_cast<float>(Viewport.Height));
  Data.FadeOriginAndRadius = FVector4(FadeOrigin, FadeRadius);
  RenderCommand::UpdateBufferData(BatchGridConstantBuffer.get(), &Data,
                                  sizeof(Data));
  RenderCommand::BindConstantBuffer(0, BatchGridConstantBuffer.get(),
                                    EShaderBindFlagBits::Vertex);
  RenderCommand::BindConstantBuffer(0, BatchGridConstantBuffer.get(),
                                    EShaderBindFlagBits::Pixel);
  RenderCommand::UpdateBufferData(BatchGridVertexBuffer.get(),
                                  BatchGridVertices,
                                  sizeof(FGridLineVertex) * VertexCount);
  RenderCommand::BindPipelineState(BatchGridPipelineState);
  RenderCommand::BindVertexBuffer(BatchGridVertexBuffer.get());
  RenderCommand::Draw(VertexCount);
}

// 직교 Grid를 평면에 고정하고 Current의 평면 관통 축은 뒤쪽·Grid·앞쪽 순서로
// 합성한다.
void FGridRenderer::OnRenderBatchGrid(const FMatrix &ViewProj,
                                      const FVector &CameraPos,
                                      const FVector &CameraForward,
                                      EGridPlane Plane, float GridSpacing,
                                      bool bDrawAllWorldAxes,
                                      const FViewportSettings &Viewport) {
  if (Viewport.Width == 0 || Viewport.Height == 0)
    return;
  FGridFrustum Frustum{};
  if (!BuildGridFrustum(ViewProj, Frustum))
    return;
  const float Spacing = GridSpacing > 0.0f ? GridSpacing : 1.0f;
  const FVector Normal = Plane == EGridPlane::XY   ? FVector(0, 0, 1)
                         : Plane == EGridPlane::XZ ? FVector(0, 1, 0)
                                                   : FVector(1, 0, 0);
  const bool Orthographic = std::fabs(ViewProj.M[0][3]) +
                                std::fabs(ViewProj.M[1][3]) +
                                std::fabs(ViewProj.M[2][3]) <
                            1.0e-6f;
  const FVector FadeOrigin(CameraPos.X, CameraPos.Y, 0);
  const float FadeRadius =
      Orthographic ? 0.0f
                   : std::clamp(std::fabs(CameraPos.Z) * 25.0f, 5.0f, 50.0f);
  const float CameraSide =
      Orthographic ? -CameraForward.Dot(Normal) : CameraPos.Dot(Normal);
  // Grid는 월드 원점에 고정하고 생성 범위만 절두체와 평면의 교차 영역을 따른다.
  const auto Position = [Plane](float U, float V) {
    if (Plane == EGridPlane::XZ)
      return FVector(U, 0, V);
    if (Plane == EGridPlane::YZ)
      return FVector(0, U, V);
    return FVector(U, V, 0);
  };
  uint32 Count = 0;
  const float NormalExtent = Plane == EGridPlane::XY   ? Frustum.AxisExtent.Z
                             : Plane == EGridPlane::XZ ? Frustum.AxisExtent.Y
                                                       : Frustum.AxisExtent.X;
  const float FrontExtent = CameraSide >= 0.0f ? NormalExtent : -NormalExtent;
  // 평면 관통 축도 월드 부호로 불투명도를 정해 카메라 반대편에서도 음수 방향을
  // 옅게 유지한다.
  const auto NormalColor = [Plane](float SignedExtent) {
    const float Alpha = SignedExtent < 0.0f ? 0.3f : 1.0f;
    if (Plane == EGridPlane::XY)
      return FVector4(0.25f, 0.45f, 0.90f, Alpha);
    return Plane == EGridPlane::XZ ? FVector4(0, 1, 0, Alpha)
                                   : FVector4(1, 0, 0, Alpha);
  };
  if (bDrawAllWorldAxes) {
    AddWorldLine(Normal * -FrontExtent, FVector(0, 0, 0),
                 NormalColor(-FrontExtent), 1.5f, Count);
    DrawWorldLines(Count, ViewProj, Viewport, FadeOrigin, FadeRadius);
    Count = 0;
  }
  float MinU, MaxU, MinV, MaxV;
  bool bHasVisibleGrid =
      GetVisibleGridBounds(Frustum, Plane, MinU, MaxU, MinV, MaxV);
  if (bHasVisibleGrid && !Orthographic) {
    // 거리 페이드 밖의 선은 보이지 않으므로 해당 영역을 제외해 회전에 따른 간격
    // 급변을 막는다. 페이드 구를 포함하는 평면 좌표 사각형으로 제한하고, 실제
    // 원형 경계는 Shader가 처리한다.
    const float CenterU = Plane == EGridPlane::YZ ? FadeOrigin.Y : FadeOrigin.X;
    const float CenterV = Plane == EGridPlane::XY ? FadeOrigin.Y : FadeOrigin.Z;
    MinU = std::max(MinU, CenterU - FadeRadius);
    MaxU = std::min(MaxU, CenterU + FadeRadius);
    MinV = std::max(MinV, CenterV - FadeRadius);
    MaxV = std::min(MaxV, CenterV + FadeRadius);
    bHasVisibleGrid = MinU <= MaxU && MinV <= MaxV;
  }
  if (bHasVisibleGrid) {
    // 극단적 줌아웃 시 생성할 선 수를 줄여 정점 버퍼 상한을 보장한다.
    double Step = static_cast<double>(Spacing) * 0.1;
    const double Span = std::max(static_cast<double>(MaxU) - MinU,
                                 static_cast<double>(MaxV) - MinV);
    while (Span / Step > 4000.0)
      Step *= 10.0;
    // 월드 좌표의 정수 배수에서 시작하므로 이동해도 격자의 위상은 바뀌지
    // 않는다.
    const auto AddGridDirection = [&](bool AlongV, double Minimum,
                                      double Maximum) {
      const double First = std::floor(Minimum / Step) - 1.0;
      const int Lines = static_cast<int>(std::ceil(Maximum / Step) - First) + 2;
      for (int Index = 0; Index < Lines; ++Index) {
        const double Lattice = First + Index;
        const bool Major = std::fabs(std::remainder(Lattice, 10.0)) < 0.1;
        const float Coordinate = static_cast<float>(Lattice * Step);
        const FVector4 Color(1, 1, 1, Major ? 0.60f : 0.25f);
        AddWorldLine(
            AlongV ? Position(Coordinate, MinV - static_cast<float>(Step))
                   : Position(MinU - static_cast<float>(Step), Coordinate),
            AlongV ? Position(Coordinate, MaxV + static_cast<float>(Step))
                   : Position(MaxU + static_cast<float>(Step), Coordinate),
            Color, Major ? 0.75f : 0.5f, Count);
      }
    };
    AddGridDirection(true, MinU, MaxU);
    AddGridDirection(false, MinV, MaxV);
  }
  AddWorldAxes(Plane, false, Frustum.AxisExtent, Count);
  DrawWorldLines(Count, ViewProj, Viewport, FadeOrigin, FadeRadius);
  if (bDrawAllWorldAxes) {
    Count = 0;
    AddWorldLine(FVector(0, 0, 0), Normal * FrontExtent,
                 NormalColor(FrontExtent), 1.5f, Count);
    DrawWorldLines(Count, ViewProj, Viewport, FadeOrigin, FadeRadius);
  }
}

// 반투명 XY Grid를 기준으로 Z축을 나눠 뒤쪽 축·Grid·앞쪽 축 순서로 합성한다.
void FGridRenderer::OnRenderPSGrid(const FMatrix &ViewProj,
                                   const FVector &CameraPos,
                                   const FEditorSettings &InEditorSettings,
                                   const FViewportSettings &Viewport) {
  FGridFrustum Frustum{};
  if (!BuildGridFrustum(ViewProj, Frustum))
    return;
  // 원근 축은 Grid의 거리 페이드를 공유하고 직교의 무한 가시 범위 정책과
  // 분리한다.
  const FVector FadeOrigin(CameraPos.X, CameraPos.Y, 0);
  const float FadeRadius =
      std::clamp(std::fabs(CameraPos.Z) * 25.0f, 5.0f, 50.0f);
  FPSGridData Data{};
  Data.invViewProj = ViewProj;
  Data.CameraPos = CameraPos;
  Data.CellSize = std::max(1, InEditorSettings.GridSpacing);
  Data.SubCellSize = Data.CellSize * 0.1f;
  // 깊이를 기록하지 않는 Grid 위로 뒤쪽 Z축이 덮이지 않도록 평면 반대편부터
  // 그린다.
  const float AxisExtent = Frustum.AxisExtent.Z;
  const float FrontZ = CameraPos.Z >= 0.0f ? AxisExtent : -AxisExtent;
  // 카메라 기준 앞뒤가 아니라 월드 Z 부호로 불투명도를 정한다.
  const FVector4 BackColor(0.25f, 0.45f, 0.90f, FrontZ > 0.0f ? 0.3f : 1.0f);
  const FVector4 FrontColor(0.25f, 0.45f, 0.90f, FrontZ > 0.0f ? 1.0f : 0.3f);
  uint32 Count = 0;
  AddWorldLine(FVector(0, 0, -FrontZ), FVector(0, 0, 0), BackColor, 1.5f,
               Count);
  DrawWorldLines(Count, ViewProj, Viewport, FadeOrigin, FadeRadius);
  RenderCommand::BindPipelineState(PSGridPipelineState);
  RenderCommand::BindVertexBuffer(nullptr);
  RenderCommand::UpdateBufferData(PSGridConstantBuffer.get(), &Data,
                                  sizeof(Data));
  RenderCommand::BindConstantBuffer(0, PSGridConstantBuffer.get(),
                                    EShaderBindFlagBits::Vertex);
  RenderCommand::BindConstantBuffer(0, PSGridConstantBuffer.get(),
                                    EShaderBindFlagBits::Pixel);
  RenderCommand::Draw(4);
  Count = 0;
  AddWorldAxes(EGridPlane::XY, false, Frustum.AxisExtent, Count);
  AddWorldLine(FVector(0, 0, 0), FVector(0, 0, FrontZ), FrontColor, 1.5f,
               Count);
  DrawWorldLines(Count, ViewProj, Viewport, FadeOrigin, FadeRadius);
}
