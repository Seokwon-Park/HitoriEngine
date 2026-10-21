#include "EnginePCH.h"
#include "StaticMeshLODGenerator.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <cstdint>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    // 원본 LOD0의 삼각형 수에 곱한다.
    constexpr double TargetRatios[4] = {
        1.0, 0.7, 0.25, 0.1
    };

    // LOD0 AABB 대각선 대비 허용 오차(평면까지의 RMS 거리). 이보다 크게 형태를 바꾸는 collapse는
    // 목표 삼각형 수에 못 미치더라도 하지 않는다. 목표를 억지로 맞추면 면이 통째로 꺼진다.
    constexpr double MaxErrorRatios[4] = {
        0.0, 0.005, 0.01, 0.025
    };

    struct FQuadric
    {
        double M[4][4]{};
        // 누적된 평면 수. Evaluate(p) / Weight가 평면까지의 평균 제곱 거리다.
        double Weight = 0.0;

        // 평면 n.x*x + n.y*y + n.z*z + d = 0의 오차를 더한다.
        // Q += [nx, ny, nz, d]^T * [nx, ny, nz, d]
        void AddPlane(const FVector& Normal, double D)
        {
            Weight += 1.0;
            const double Plane[4] = {
                Normal.X, Normal.Y, Normal.Z, D
            };

            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    M[I][J] += Plane[I] * Plane[J];
                }
            }
        }

        FQuadric& operator+=(const FQuadric& Other)
        {
            Weight += Other.Weight;
            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    M[I][J] += Other.M[I][J];
                }
            }
            return *this;
        }

        // 위치 p에서의 QEM 비용: [px, py, pz, 1] Q [px, py, pz, 1]^T
        double Evaluate(const FVector& Position) const
        {
            const double P[4] = {
                Position.X, Position.Y, Position.Z, 1.0
            };

            double Cost = 0.0;
            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    Cost += P[I] * M[I][J] * P[J];
                }
            }
            return Cost;
        }
    };

    bool BuildVertexQuadrics(const FStaticMeshData& Mesh,std::vector<FQuadric>& OutQuadrics, FString& OutError)
    {
        OutQuadrics.assign(static_cast<size_t>(Mesh.Vertices.Num()), FQuadric{});

        for (uint32 Index = 0; Index < static_cast<uint32>(Mesh.Indices.Num()); Index += 3)
        {
            const uint32 IA = Mesh.Indices[Index];
            const uint32 IB = Mesh.Indices[Index + 1];
            const uint32 IC = Mesh.Indices[Index + 2];

            const FVector A = Mesh.Vertices[IA].Position;
            const FVector B = Mesh.Vertices[IB].Position;
            const FVector C = Mesh.Vertices[IC].Position;

            const FVector Cross = FVector::Cross(B - A, C - A);

            const float LengthSquared = Cross.Dot(Cross);
            if (LengthSquared <= 1e-20f)
            {
                OutError = "LOD0 contains a degenerate triangle";
                return false;
            }

            const FVector Normal = Cross * (1.0f / std::sqrt(LengthSquared));

            const double D = -static_cast<double>(Normal.Dot(A));

            // 이 면을 공유하는 세 정점에 같은 평면 오차를 더한다.
            OutQuadrics[IA].AddPlane(Normal, D);
            OutQuadrics[IB].AddPlane(Normal, D);
            OutQuadrics[IC].AddPlane(Normal, D);
        }
        return true;
    }

    struct FEdgePlacement
    {
        FVector Position;
        float T = 0.0f;  // A + T * (B - A)
        double Cost = 0.0;
    };

    FEdgePlacement FindBestPointOnEdge(const FVector& A,const FVector& B, const FQuadric& QA, const FQuadric& QB)
    {
        // 두 정점을 하나로 합쳤을 때의 오차 행렬
        FQuadric Q = QA;
        Q += QB;

        const FVector Direction = B - A;

        // P(t) = A + t(B-A), 0 <= t <= 1
        // E(t) = P(t)^T Q P(t)
        //      = Quadratic*t*t + Linear*t + Constant
        const double P0[4] = { A.X, A.Y, A.Z, 1.0 };
        const double Delta[4] = { Direction.X, Direction.Y, Direction.Z, 0.0 };

        double Quadratic = 0.0;
        double Linear = 0.0;

        for (int I = 0; I < 4; ++I)
        {
            for (int J = 0; J < 4; ++J)
            {
                Quadratic += Delta[I] * Q.M[I][J] * Delta[J];

                Linear += 2.0 * P0[I] * Q.M[I][J] * Delta[J];
            }
        }

        auto MakePlacement = [&](float T)
            {
                const FVector Position = A + Direction * T;
                return FEdgePlacement{ Position, T, Q.Evaluate(Position) };
            };

        // 양 끝점도 반드시 후보에 넣는다.
        FEdgePlacement Best = MakePlacement(0.0f);

        const FEdgePlacement End = MakePlacement(1.0f);
        if (End.Cost < Best.Cost) Best = End;

        // 이차 함수의 최소점이 있으면 edge 범위 [0,1]로 제한한다.
        if (Quadratic > 1e-20)
        {
            const double UnclampedT = -Linear / (2.0 * Quadratic);
            const float T = static_cast<float>(std::clamp(UnclampedT, 0.0, 1.0));
            const FEdgePlacement Middle = MakePlacement(T);

            if (Middle.Cost < Best.Cost) Best = Middle;
        }
        return Best;
    }

    bool TryFindBestPointOnQEM(const FVector& A, const FVector& B,
        const FQuadric& QA, const FQuadric& QB, FEdgePlacement& Out)
    {
        FQuadric Q = QA;
        Q += QB;

        // Q의 왼쪽 위 3x3에 대해 H * p = -b를 푼다.
        double M[3][4]{};
        double Scale = 0.0;
        for (int I = 0; I < 3; ++I)
        {
            for (int J = 0; J < 3; ++J)
            {
                M[I][J] = Q.M[I][J];
                Scale = std::max(Scale, std::abs(M[I][J]));
            }
            M[I][3] = -Q.M[I][3];
        }

        if (!std::isfinite(Scale) || Scale == 0.0) return false;

        for (int Col = 0; Col < 3; ++Col)
        {
            int Pivot = Col;
            for (int Row = Col + 1; Row < 3; ++Row)
            {
                if (std::abs(M[Row][Col]) > std::abs(M[Pivot][Col])) Pivot = Row;
            }
            if (std::abs(M[Pivot][Col]) <= Scale * 1e-12) return false;

            if (Pivot != Col)
            {
                for (int J = 0; J < 4; ++J)
                    std::swap(M[Col][J], M[Pivot][J]);
            }

            for (int Row = Col + 1; Row < 3; ++Row)
            {
                const double Factor = M[Row][Col] / M[Col][Col];
                for (int J = Col; J < 4; ++J)
                    M[Row][J] -= Factor * M[Col][J];
            }
        }

        double X[3]{};
        for (int I = 2; I >= 0; --I)
        {
            double Value = M[I][3];
            for (int J = I + 1; J < 3; ++J)
                Value -= M[I][J] * X[J];
            X[I] = Value / M[I][I];
            if (!std::isfinite(X[I])) return false;
        }

        const FVector Position(
            static_cast<float>(X[0]),
            static_cast<float>(X[1]),
            static_cast<float>(X[2]));
        if (!std::isfinite(Position.X) ||
            !std::isfinite(Position.Y) ||
            !std::isfinite(Position.Z))
            return false;

        // 간선 밖 위치에서도 UV와 Color는 간선에 투영한 T로 보간한다.
        const FVector Direction = B - A;
        const float LengthSquared = Direction.Dot(Direction);
        const float T = LengthSquared > 1e-20f
            ? std::clamp((Position - A).Dot(Direction) / LengthSquared, 0.0f, 1.0f)
            : 0.0f;

        Out = { Position, T, Q.Evaluate(Position) };
        return std::isfinite(Out.Cost);
    }

    struct FWorkTriangle
    {
        uint32 V[3]{};
        uint32 MaterialSlotIndex = 0;
        bool bAlive = true;
    };

    struct FWorkVertex
    {
        FVertex Vertex;
        FQuadric Quadric;

        // 고정된 특징점이 collapse의 도착점이 될 때 원본 위치 ID를 유지한다.
        // UV seam의 분리된 렌더 정점을 LOD 법선 계산에서 다시 찾는 데 쓴다.
        int32 SourcePositionIndex = -1;

        // 이 정점을 사용하는 삼각형 ID
        std::unordered_set<uint32> Faces;

        // 삼각형의 edge로 연결된 정점 ID
        std::unordered_set<uint32> Neighbors;

        // UV·노멀·재질 이음매에서 같은 원본 위치를 공유하는 다른 쪽 렌더 정점.
        // 이음선을 따라 접을 때 두 정점을 같은 위치로 함께 옮겨 틈이 생기지 않게 한다.
        uint32 Twin = std::numeric_limits<uint32>::max();

        uint32 Revision = 0;
        // 혼자서는 움직일 수 없는 정점. 다른 정점이 이 위치로 합쳐지는 것은 허용한다.
        bool bProtected = false;
        // 이음매 쌍으로도 움직일 수 없는 정점 (3개 이상 겹침, 열린 경계, 비다양체, 날카로운 모서리)
        bool bLocked = false;
        bool bAlive = true;
    };

    constexpr uint32 InvalidVertex = std::numeric_limits<uint32>::max();

    struct FWorkMesh
    {
        std::vector<FWorkVertex> Vertices;
        std::vector<FWorkTriangle> Triangles;
        uint32 LiveTriangles = 0;
    };

    // LOD0을 작업용 자료구조로 복사
    bool BuildWorkMesh(const FStaticMeshData& Source, const std::vector<FQuadric>& Quadrics, FWorkMesh& Out, FString& OutError)
    {
        const uint32 VertexCount = static_cast<uint32>(Source.Vertices.Num());
        const uint32 TriangleCount = static_cast<uint32>(Source.Indices.Num() / 3);

        Out.Vertices.resize(VertexCount);
        Out.Triangles.resize(TriangleCount);
        Out.LiveTriangles = TriangleCount;

        for (uint32 I = 0; I < VertexCount; ++I)
        {
            Out.Vertices[I].Vertex = Source.Vertices[I];
            Out.Vertices[I].Quadric = Quadrics[I];
            Out.Vertices[I].SourcePositionIndex = Source.LODSourceVertices[I].PositionIndex;
        }

        // 원본 삼각형 하나가 어느 material section에 속하는지 기록한다.
        std::vector<int32> TriangleMaterials(TriangleCount, -1);

        for (const FStaticMeshSection& Section : Source.Sections)
        {
            if (Section.StartIndex % 3 != 0 ||
                Section.IndexCount % 3 != 0)
            {
                OutError = "Section range is not triangle-aligned";
                return false;
            }

            const uint32 End = Section.StartIndex + Section.IndexCount;

            for (uint32 Index = Section.StartIndex; Index < End; Index += 3)
            {
                const uint32 TriangleID = Index / 3;

                if (TriangleID >= TriangleCount || TriangleMaterials[TriangleID] != -1)
                {
                    OutError = "Sections overlap or exceed triangle range";
                    return false;
                }

                TriangleMaterials[TriangleID] = static_cast<int32>(Section.MaterialSlotIndex);
            }
        }

        for (uint32 TriangleID = 0; TriangleID < TriangleCount; ++TriangleID)
        {
            if (TriangleMaterials[TriangleID] < 0)
            {
                OutError = "A triangle has no material section";
                return false;
            }

            FWorkTriangle& Face = Out.Triangles[TriangleID];
            Face.MaterialSlotIndex = static_cast<uint32>(TriangleMaterials[TriangleID]);

            for (uint32 Corner = 0; Corner < 3; ++Corner)
            {
                Face.V[Corner] = Source.Indices[TriangleID * 3 + Corner];
                Out.Vertices[Face.V[Corner]].Faces.insert(TriangleID);
            }

            // 세 edge에 대한 이웃 관계를 양방향으로 기록한다.
            for (uint32 Edge = 0; Edge < 3; ++Edge)
            {
                const uint32 A = Face.V[Edge];
                const uint32 B = Face.V[(Edge + 1) % 3];

                Out.Vertices[A].Neighbors.insert(B);
                Out.Vertices[B].Neighbors.insert(A);
            }
        }
        return true;
    }

    uint64 MakeGeometricEdgeKey(uint32 A, uint32 B)
    {
        if (A > B) std::swap(A, B);

        return (uint64(A) << 32) | uint64(B);
    }

    struct FEdgeUse
    {
        uint32 TriangleID = 0;
        uint32 RenderA = 0;
        uint32 RenderB = 0;
    };

    FVector GetFaceNormal(const FWorkTriangle& Face,const FWorkMesh& Work)
    {
        const FVector& A = Work.Vertices[Face.V[0]].Vertex.Position;
        const FVector& B = Work.Vertices[Face.V[1]].Vertex.Position;
        const FVector& C = Work.Vertices[Face.V[2]].Vertex.Position;

        return FVector::Cross(B - A, C - A).Normalized();
    }

    void MarkProtectedVertices(const FStaticMeshData& Source, FWorkMesh& Work,
        uint32 LOD, FLODGenerateResult& Result)
    {
        constexpr float HardEdgeCosine[4] = { 0.0f, 0.6f, 0.6f, 0.0f };

        for (FWorkVertex& Vertex : Work.Vertices)
        {
            if (!Vertex.bAlive) continue;
            Vertex.bProtected = false;
            Vertex.bLocked = false;
            Vertex.Twin = InvalidVertex;
        }

        auto Lock = [&Work](uint32 V)
            {
                Work.Vertices[V].bProtected = true;
                Work.Vertices[V].bLocked = true;
            };

        // 현재 살아 있는 렌더 정점을 원본 기하 위치별로 묶는다.
        // 원본 위치를 잃은 정점은 각각 별개의 기하 위치로 취급한다.
        std::unordered_map<int32, uint32> SourceToGeometry;
        std::vector<uint32> GeometryID(Work.Vertices.size());
        uint32 NextGeometryID = 0;
        for (uint32 I = 0; I < Work.Vertices.size(); ++I)
        {
            if (!Work.Vertices[I].bAlive || Work.Vertices[I].Faces.empty())
                continue;

            const int32 SourceID = Work.Vertices[I].SourcePositionIndex;
            if (SourceID < 0)
            {
                GeometryID[I] = NextGeometryID++;
            }
            else
            {
                auto [It, bInserted] = SourceToGeometry.try_emplace(SourceID, NextGeometryID);
                if (bInserted) ++NextGeometryID;
                GeometryID[I] = It->second;
            }
        }

        std::vector<uint32> GeometryVertexCounts(NextGeometryID, 0);
        std::vector<uint32> GeometryFirstVertex(NextGeometryID, InvalidVertex);
        for (uint32 I = 0; I < Work.Vertices.size(); ++I)
        {
            if (!Work.Vertices[I].bAlive || Work.Vertices[I].Faces.empty())
                continue;

            const uint32 Geometry = GeometryID[I];
            if (++GeometryVertexCounts[Geometry] == 1)
                GeometryFirstVertex[Geometry] = I;
        }

        // 같은 위치에 렌더 정점이 여러 개면(UV·노멀·재질 이음매) 모든 LOD에서 혼자 움직이지 못하게 한다.
        // 한쪽만 움직이면 이음선이 벌어진다. 정확히 2개면 쌍둥이로 묶어 이음선을 따라 함께 접을 수 있게 하고,
        // 3개 이상이면 이음선이 만나는 교차점이라 고정한다.
        for (uint32 I = 0; I < Work.Vertices.size(); ++I)
        {
            if (!Work.Vertices[I].bAlive || Work.Vertices[I].Faces.empty())
                continue;

            const uint32 Geometry = GeometryID[I];
            const uint32 Count = GeometryVertexCounts[Geometry];
            if (Count < 2) continue;

            Work.Vertices[I].bProtected = true;
            if (Count >= 3)
            {
                Work.Vertices[I].bLocked = true;
                continue;
            }

            const uint32 First = GeometryFirstVertex[Geometry];
            if (First != I)
            {
                Work.Vertices[I].Twin = First;
                Work.Vertices[First].Twin = I;
            }
        }

        std::unordered_map<uint64, std::vector<FEdgeUse>> EdgeUses;

        // 현재 살아 있는 삼각형의 기하 edge만 수집한다.
        for (uint32 TriangleID = 0; TriangleID < Work.Triangles.size(); ++TriangleID)
        {
            const FWorkTriangle& Face = Work.Triangles[TriangleID];
            if (!Face.bAlive) continue;

            for (uint32 Edge = 0; Edge < 3; ++Edge)
            {
                const uint32 RenderA = Face.V[Edge];
                const uint32 RenderB = Face.V[(Edge + 1) % 3];
                const uint32 GeometryA = GeometryID[RenderA];
                const uint32 GeometryB = GeometryID[RenderB];

                if (GeometryA == GeometryB)
                {
                    Lock(RenderA);
                    Lock(RenderB);
                    continue;
                }

                const uint64 Key = MakeGeometricEdgeKey(GeometryA, GeometryB);
                EdgeUses[Key].push_back({ TriangleID, RenderA, RenderB });
            }
        }

        for (const auto& [Key, Uses] : EdgeUses)
        {
            // 비다양체와 열린 경계는 모든 LOD에서 고정한다.
            bool bLockEdge = Uses.size() >= 3 || Uses.size() == 1;
            bool bSeamEdge = false;

            if (Uses.size() == 2)
            {
                const FEdgeUse& U0 = Uses[0];
                const FEdgeUse& U1 = Uses[1];

                const FWorkTriangle& F0 = Work.Triangles[U0.TriangleID];
                const FWorkTriangle& F1 = Work.Triangles[U1.TriangleID];

                const uint32 GeometryA = static_cast<uint32>(Key >> 32);
                const uint32 GeometryB = static_cast<uint32>(Key);
                auto RenderAt = [&](const FEdgeUse& Use, uint32 Geometry)
                    {
                        return GeometryID[Use.RenderA] == Geometry ? Use.RenderA : Use.RenderB;
                    };

                const uint32 A0 = RenderAt(U0, GeometryA);
                const uint32 A1 = RenderAt(U1, GeometryA);
                const uint32 B0 = RenderAt(U0, GeometryB);
                const uint32 B1 = RenderAt(U1, GeometryB);

                const bool bMaterialBoundary = F0.MaterialSlotIndex != F1.MaterialSlotIndex;
                const bool bUVSeam =
                    Source.LODSourceVertices[A0].UVIndex != Source.LODSourceVertices[A1].UVIndex ||
                    Source.LODSourceVertices[B0].UVIndex != Source.LODSourceVertices[B1].UVIndex;

                const bool bNormalDiscontinuity =
                    (Source.LODSourceVertices[A0].NormalIndex >= 0 &&
                        Source.LODSourceVertices[A1].NormalIndex >= 0 &&
                        Source.LODSourceVertices[A0].NormalIndex != Source.LODSourceVertices[A1].NormalIndex) ||
                    (Source.LODSourceVertices[B0].NormalIndex >= 0 &&
                        Source.LODSourceVertices[B1].NormalIndex >= 0 &&
                        Source.LODSourceVertices[B0].NormalIndex != Source.LODSourceVertices[B1].NormalIndex);

                const float FaceDot = GetFaceNormal(F0, Work).Dot(GetFaceNormal(F1, Work));
                // 이음매는 쌍둥이 collapse로 줄일 수 있고, 날카로운 모서리는 형태 유지를 위해 고정한다.
                bSeamEdge = bMaterialBoundary || bUVSeam || bNormalDiscontinuity;
                bLockEdge = bLockEdge || FaceDot < HardEdgeCosine[LOD];
            }

            if (!bLockEdge && !bSeamEdge) continue;

            ++Result.RejectedFeatureEdges;

            // edge 양쪽에서 사용된 렌더 정점 모두 고정한다.
            for (const FEdgeUse& Use : Uses)
            {
                if (bLockEdge)
                {
                    Lock(Use.RenderA);
                    Lock(Use.RenderB);
                }
                else
                {
                    Work.Vertices[Use.RenderA].bProtected = true;
                    Work.Vertices[Use.RenderB].bProtected = true;
                }
            }
        }
    }

    struct FCollapseCandidate
    {
        uint32 A = 0;
        uint32 B = 0;
        uint32 RevisionA = 0;
        uint32 RevisionB = 0;
        FEdgePlacement Placement;

        // 이음선을 따라 접는 경우 반대쪽 쌍둥이 edge(TwinA-TwinB)도 같은 위치로 함께 접는다.
        uint32 TwinA = InvalidVertex;
        uint32 TwinB = InvalidVertex;
        uint32 RevisionTwinA = 0;
        uint32 RevisionTwinB = 0;

        // 합쳐질 모든 평면까지의 평균 제곱 거리. LOD별 허용 오차와 비교한다.
        double ErrorSquared = 0.0;

        bool IsSeamPair() const { return TwinA != InvalidVertex; }
    };

    struct FGreaterCollapseCost
    {
        bool operator()(const FCollapseCandidate& L,const FCollapseCandidate& R) const
        {
            if (L.Placement.Cost != R.Placement.Cost)
                return L.Placement.Cost > R.Placement.Cost;

            // 같은 비용의 처리 순서를 고정한다.
            return std::tie(L.A, L.B) > std::tie(R.A, R.B);
        }
    };

    using FCollapseHeap = std::priority_queue<FCollapseCandidate, std::vector<FCollapseCandidate>, FGreaterCollapseCost>;

    std::vector<uint32> SharedFaces(uint32 A, uint32 B, const FWorkMesh& Work)
    {
        std::vector<uint32> Result;
        for (uint32 FaceID : Work.Vertices[A].Faces)
        {
            if (Work.Triangles[FaceID].bAlive && Work.Vertices[B].Faces.contains(FaceID))
            {
                Result.push_back(FaceID);
            }
        }
        return Result;
    }

    bool PassesLinkCondition(uint32 A,uint32 B, const FWorkMesh& Work)
    {
        const std::vector<uint32> Shared = SharedFaces(A, B, Work);

        if ((Shared.size() != 1 && Shared.size() != 2) ||
            Shared.size() >= Work.LiveTriangles)
            return false;

        std::unordered_set<uint32> OppositeVertices;

        for (uint32 FaceID : Shared)
        {
            const FWorkTriangle& Face = Work.Triangles[FaceID];

            for (uint32 V : Face.V)
            {
                if (V != A && V != B) OppositeVertices.insert(V);
            }
        }

        if (OppositeVertices.size() != Shared.size()) return false;

        std::unordered_set<uint32> CommonNeighbors;

        for (uint32 Neighbor : Work.Vertices[A].Neighbors)
        {
            if (Work.Vertices[B].Neighbors.contains(Neighbor))
            {
                CommonNeighbors.insert(Neighbor);
            }
        }
        return CommonNeighbors == OppositeVertices;
    }

    enum class ECollapseCheck
    {
        Allowed,
        Topology,
        Flip
    };

    // A-B 한 쪽 edge만 검사한다. 이음매 쌍은 CheckCollapse가 양쪽에 대해 호출한다.
    ECollapseCheck CheckCollapseSide(uint32 A, uint32 B, const FVector& NewPosition, const FWorkMesh& Work)
    {
        if (!PassesLinkCondition(A, B, Work)) return ECollapseCheck::Topology;

        std::unordered_set<uint32> IncidentFaces = Work.Vertices[A].Faces;

        IncidentFaces.insert(Work.Vertices[B].Faces.begin(), Work.Vertices[B].Faces.end());

        for (uint32 FaceID : IncidentFaces)
        {
            const FWorkTriangle& Face = Work.Triangles[FaceID];

            if (!Face.bAlive) continue;

            const bool bHasA = Face.V[0] == A || Face.V[1] == A || Face.V[2] == A;
            const bool bHasB = Face.V[0] == B || Face.V[1] == B || Face.V[2] == B;

            // A-B를 공유하는 면은 제거되므로 검사하지 않는다.
            if (bHasA && bHasB) continue;

            FVector OldPositions[3];
            FVector NewPositions[3];

            for (int I = 0; I < 3; ++I)
            {
                const uint32 V = Face.V[I];
                OldPositions[I] = Work.Vertices[V].Vertex.Position;

                NewPositions[I] = (V == A || V == B)
                    ? NewPosition : OldPositions[I];
            }

            const FVector OldCross = FVector::Cross(
                OldPositions[1] - OldPositions[0],
                OldPositions[2] - OldPositions[0]);

            const FVector NewCross = FVector::Cross(
                NewPositions[1] - NewPositions[0],
                NewPositions[2] - NewPositions[0]);

            if (NewCross.Dot(NewCross) <= 1e-20f ||
                OldCross.Dot(NewCross) <= 0.0f)
            {
                return ECollapseCheck::Flip;
            }
        }

        return ECollapseCheck::Allowed;
    }

    ECollapseCheck CheckCollapse(const FCollapseCandidate& Candidate, const FWorkMesh& Work)
    {
        const ECollapseCheck Check = CheckCollapseSide(Candidate.A, Candidate.B, Candidate.Placement.Position, Work);
        if (Check != ECollapseCheck::Allowed || !Candidate.IsSeamPair())
            return Check;

        // 쌍둥이 쪽 면들도 같은 새 위치에서 뒤집히거나 위상이 깨지면 안 된다.
        return CheckCollapseSide(Candidate.TwinA, Candidate.TwinB, Candidate.Placement.Position, Work);
    }

    bool IsMovableSeam(const FWorkVertex& Vertex, const FWorkMesh& Work)
    {
        return Vertex.bProtected && !Vertex.bLocked &&
            Vertex.Twin != InvalidVertex && Work.Vertices[Vertex.Twin].bAlive;
    }

    // A-B가 이음선을 따라가는 edge이고 반대쪽에 같은 모양의 쌍둥이 edge(TwinA-TwinB)가 있으면 true.
    // 이음선을 따라가는 렌더 edge는 한쪽 면만 공유한다(반대쪽 면은 쌍둥이 정점에 붙어 있다).
    bool FindSeamTwinEdge(uint32 A, uint32 B, const FWorkMesh& Work, uint32& OutTwinA, uint32& OutTwinB)
    {
        const FWorkVertex& VA = Work.Vertices[A];
        const FWorkVertex& VB = Work.Vertices[B];
        if (!IsMovableSeam(VA, Work) || !IsMovableSeam(VB, Work))
            return false;

        const uint32 TwinA = VA.Twin;
        const uint32 TwinB = VB.Twin;
        if (TwinA == B || TwinB == A || TwinA == TwinB)
            return false;

        const FWorkVertex& VTA = Work.Vertices[TwinA];
        const FWorkVertex& VTB = Work.Vertices[TwinB];
        if (!VTA.Neighbors.contains(TwinB))
            return false;

        // 쌍둥이는 같은 위치여야 한다. 어긋나 있으면 이미 벌어진 것이므로 건드리지 않는다.
        constexpr float SamePositionDistanceSquared = 1e-12f;
        const FVector DeltaA = VTA.Vertex.Position - VA.Vertex.Position;
        const FVector DeltaB = VTB.Vertex.Position - VB.Vertex.Position;
        if (DeltaA.Dot(DeltaA) > SamePositionDistanceSquared || DeltaB.Dot(DeltaB) > SamePositionDistanceSquared)
            return false;

        if (SharedFaces(A, B, Work).size() != 1 || SharedFaces(TwinA, TwinB, Work).size() != 1)
            return false;

        OutTwinA = TwinA;
        OutTwinB = TwinB;
        return true;
    }

    bool FindBestValidCandidate(uint32 A, uint32 B, const FWorkMesh& Work,
        FCollapseCandidate& Out)
    {
        const FWorkVertex& VA = Work.Vertices[A];
        const FWorkVertex& VB = Work.Vertices[B];

        Out = FCollapseCandidate{};
        Out.A = A;
        Out.B = B;
        Out.RevisionA = VA.Revision;
        Out.RevisionB = VB.Revision;

        const FVector& PositionA = VA.Vertex.Position;
        const FVector& PositionB = VB.Vertex.Position;

        bool bFound = false;
        auto Consider = [&](const FEdgePlacement& Placement)
            {
                if (!std::isfinite(Placement.Cost)) return;

                FCollapseCandidate Probe = Out;
                Probe.Placement = Placement;
                if (CheckCollapse(Probe, Work) != ECollapseCheck::Allowed)
                    return;

                if (!bFound || Placement.Cost < Out.Placement.Cost)
                {
                    Out.Placement = Placement;
                    bFound = true;
                }
            };

        // 둘 다 고정이면 이음선을 따라 쌍둥이와 함께 접는 경우만 허용한다.
        // 양쪽이 같은 위치로 이동하므로 이음선이 벌어지지 않고, UV는 각 쪽이 자기 값으로 보간한다.
        if (VA.bProtected && VB.bProtected)
        {
            uint32 TwinA = InvalidVertex;
            uint32 TwinB = InvalidVertex;
            if (!FindSeamTwinEdge(A, B, Work, TwinA, TwinB))
                return false;

            Out.TwinA = TwinA;
            Out.TwinB = TwinB;
            Out.RevisionTwinA = Work.Vertices[TwinA].Revision;
            Out.RevisionTwinB = Work.Vertices[TwinB].Revision;

            // 한 원본 위치의 오차는 이음매 양쪽 면을 모두 합친 것이다.
            FQuadric QA = VA.Quadric;
            QA += Work.Vertices[TwinA].Quadric;
            FQuadric QB = VB.Quadric;
            QB += Work.Vertices[TwinB].Quadric;
            FQuadric Q = QA;
            Q += QB;

            // 이음선 밖으로 벗어나지 않도록 edge 위의 위치만 쓴다.
            Consider(FindBestPointOnEdge(PositionA, PositionB, QA, QB));
            Consider({ PositionA, 0.0f, Q.Evaluate(PositionA) });
            Consider({ PositionB, 1.0f, Q.Evaluate(PositionB) });
            if (bFound)
                Out.ErrorSquared = Out.Placement.Cost / std::max(Q.Weight, 1.0);
            return bFound;
        }

        FQuadric Q = VA.Quadric;
        Q += VB.Quadric;

        if (VA.bProtected || VB.bProtected)
        {
            const bool bPinA = VA.bProtected;
            const FVector& Position = bPinA ? PositionA : PositionB;
            Consider({ Position, bPinA ? 0.0f : 1.0f, Q.Evaluate(Position) });
        }
        else
        {
            // 열린 렌더 경계는 간선 밖의 QEM 위치로 옮기지 않는다.
            if (SharedFaces(A, B, Work).size() != 1)
            {
                FEdgePlacement QEMPlacement;
                if (TryFindBestPointOnQEM(PositionA, PositionB,
                    VA.Quadric, VB.Quadric, QEMPlacement))
                    Consider(QEMPlacement);
            }

            Consider(FindBestPointOnEdge(PositionA, PositionB,
                VA.Quadric, VB.Quadric));
            Consider({ PositionA, 0.0f, Q.Evaluate(PositionA) });
            Consider({ PositionB, 1.0f, Q.Evaluate(PositionB) });
        }

        if (bFound)
            Out.ErrorSquared = Out.Placement.Cost / std::max(Q.Weight, 1.0);
        return bFound;
    }

    void PushCandidate(uint32 A, uint32 B, const FWorkMesh& Work, FCollapseHeap& Heap)
    {
        if (A > B) std::swap(A, B);

        const FWorkVertex& VA = Work.Vertices[A];
        const FWorkVertex& VB = Work.Vertices[B];
        if (!VA.bAlive || !VB.bAlive || !VA.Neighbors.contains(B)) return;

        FCollapseCandidate Candidate;
        if (!FindBestValidCandidate(A, B, Work, Candidate)) return;

        Heap.push(Candidate);
    }

    // B를 A에 합친다. 이웃 재구성과 힙 갱신은 ApplyCollapse가 양쪽 collapse를 끝낸 뒤 한 번에 한다.
    void CollapseEdge(uint32 A, uint32 B, const FEdgePlacement& Placement, FWorkMesh& Work,
        std::unordered_set<uint32>& Touched)
    {
        const float T = Placement.T;

        FWorkVertex& VA = Work.Vertices[A];
        FWorkVertex& VB = Work.Vertices[B];

        // 변경 전 이웃을 기억한다. 변경 후 이 영역의 후보만 갱신.
        Touched.insert(A);
        Touched.insert(B);
        Touched.insert(VA.Neighbors.begin(), VA.Neighbors.end());
        Touched.insert(VB.Neighbors.begin(), VB.Neighbors.end());

        const FVertex OldA = VA.Vertex;
        const FVertex OldB = VB.Vertex;

        if (VB.bProtected)
        {
            // A가 B 위치에 고정되면 seam과 원래의 smoothing 정보도 B를 따른다.
            VA.SourcePositionIndex = VB.SourcePositionIndex;
            VA.Vertex.Normal = OldB.Normal;

            if (!VA.bProtected)
            {
                // 내부 정점 A가 이음매 정점 B의 자리를 물려받는다. B의 쌍둥이가 이제 A를 가리키게 한다.
                VA.bLocked = VB.bLocked;
                VA.Twin = VB.Twin;
                if (VB.Twin != InvalidVertex)
                {
                    Work.Vertices[VB.Twin].Twin = A;
                    Touched.insert(VB.Twin);
                }
            }
        }
        else if (!VA.bProtected)
        {
            // 이동한 내부 정점은 더 이상 원본 위치의 seam 정점이 아니다.
            VA.SourcePositionIndex = -1;
        }

        VA.Vertex.Position = Placement.Position;
        VA.Vertex.UV = OldA.UV * (1.0f - T) + OldB.UV * T;
        VA.Vertex.Color = OldA.Color * (1.0f - T) + OldB.Color * T;

        // Normal은 결과 LOD를 만들 때 재계산한다.
        VA.Quadric += VB.Quadric;
        VA.bProtected = VA.bProtected || VB.bProtected;

        // 순회 도중 VB.Faces를 수정하므로 목록을 복사한다.
        const std::vector<uint32> BFaces(VB.Faces.begin(), VB.Faces.end());

        for (uint32 FaceID : BFaces)
        {
            FWorkTriangle& Face = Work.Triangles[FaceID];

            if (!Face.bAlive) continue;

            bool bContainsA = false;
            for (uint32 V : Face.V) 
                bContainsA |= V == A;

            if (bContainsA)
            {
                // Collapse로 퇴화하는 두 면을 제거한다.
                Face.bAlive = false;
                --Work.LiveTriangles;

                for (uint32 V : Face.V)
                    Work.Vertices[V].Faces.erase(FaceID);
            }
            else
            {
                // 살아남는 면에서 B -> A 치환
                for (uint32& V : Face.V)
                {
                    if (V == B) V = A;
                }

                VA.Faces.insert(FaceID);
                VB.Faces.erase(FaceID);
            }
        }

        VB.Faces.clear();
        VB.Neighbors.clear();
        VB.bAlive = false;
    }

    void ApplyCollapse(const FCollapseCandidate& Candidate, FWorkMesh& Work, FCollapseHeap& Heap)
    {
        std::unordered_set<uint32> Touched;
        CollapseEdge(Candidate.A, Candidate.B, Candidate.Placement, Work, Touched);

        // 이음선 반대쪽도 같은 위치·같은 T로 접는다. 위치는 같아지고 UV는 각 쪽의 값으로 보간된다.
        // A와 TwinA는 계속 쌍둥이로 남는다.
        if (Candidate.IsSeamPair())
            CollapseEdge(Candidate.TwinA, Candidate.TwinB, Candidate.Placement, Work, Touched);

        // Touched 정점의 이웃 관계를 살아 있는 면에서 재구성.
        for (uint32 V : Touched)
        {
            FWorkVertex& Vertex = Work.Vertices[V];
            Vertex.Neighbors.clear();

            if (!Vertex.bAlive)
                continue;

            for (uint32 FaceID : Vertex.Faces)
            {
                const FWorkTriangle& Face =
                    Work.Triangles[FaceID];

                if (!Face.bAlive)
                    continue;

                for (uint32 Other : Face.V)
                {
                    if (Other != V)
                        Vertex.Neighbors.insert(Other);
                }
            }

            ++Vertex.Revision;
        }

        // 이전 힙 항목은 Revision 불일치로 폐기된다.
        // 현재 주변의 edge만 새 비용으로 다시 넣는다.
        for (uint32 V : Touched)
        {
            if (!Work.Vertices[V].bAlive)
                continue;

            for (uint32 Neighbor :
            Work.Vertices[V].Neighbors)
            {
                PushCandidate(V, Neighbor, Work, Heap);
            }
        }
    }

    // 원점 기준 사면체 부피의 합. 닫힌 메시면 메시 부피가 된다.
    double ComputeSignedVolume(const FStaticMeshData& Mesh)
    {
        double Volume = 0.0;
        for (uint32 I = 0; I + 2 < static_cast<uint32>(Mesh.Indices.Num()); I += 3)
        {
            const FVector& A = Mesh.Vertices[Mesh.Indices[I]].Position;
            const FVector& B = Mesh.Vertices[Mesh.Indices[I + 1]].Position;
            const FVector& C = Mesh.Vertices[Mesh.Indices[I + 2]].Position;
            Volume += static_cast<double>(A.Dot(FVector::Cross(B, C)));
        }
        return Volume / 6.0;
    }

    FStaticMeshData BuildLODMesh(const FWorkMesh& Work,const FStaticMeshData& LOD0)
    {
        FStaticMeshData Out;
        Out.MaterialSlots = LOD0.MaterialSlots;

        constexpr uint32 Invalid = std::numeric_limits<uint32>::max();

        std::vector<uint32> Remap(Work.Vertices.size(), Invalid);

        // 재질별로 인덱스를 모아 Section을 연속 범위로 만든다.
        for (uint32 Slot = 0; Slot < static_cast<uint32>(Out.MaterialSlots.Num()); ++Slot)
        {
            FStaticMeshSection Section;
            Section.StartIndex = static_cast<uint32>(Out.Indices.Num());
            Section.MaterialSlotIndex = Slot;

            for (const FWorkTriangle& Face : Work.Triangles)
            {
                if (!Face.bAlive || Face.MaterialSlotIndex != Slot)
                    continue;

                for (uint32 OldIndex : Face.V)
                {
                    if (Remap[OldIndex] == Invalid)
                    {
                        Remap[OldIndex] = static_cast<uint32>(Out.Vertices.Num());
                        Out.Vertices.Add(Work.Vertices[OldIndex].Vertex);
                    }

                    Out.Indices.Add(Remap[OldIndex]);
                }
            }

            Section.IndexCount = static_cast<uint32>(Out.Indices.Num()) - Section.StartIndex;
            if (Section.IndexCount > 0) Out.Sections.Add(Section);
        }

        // 면적 가중 normal을 계산한다. UV seam에서는 같은 원본 위치에
        // 여러 렌더 정점이 있으므로, 원본에서 매끈했던 정점끼리만 합친다.
        std::vector<FVector> NormalSums(Out.Vertices.Num(), FVector());

        for (uint32 I = 0;I < static_cast<uint32>( Out.Indices.Num());I += 3)
        {
            const uint32 IA = Out.Indices[I];
            const uint32 IB = Out.Indices[I + 1];
            const uint32 IC = Out.Indices[I + 2];

            const FVector& A = Out.Vertices[IA].Position;
            const FVector& B = Out.Vertices[IB].Position;
            const FVector& C = Out.Vertices[IC].Position;

            const FVector Cross = FVector::Cross(B - A, C - A);

            NormalSums[IA] += Cross;
            NormalSums[IB] += Cross;
            NormalSums[IC] += Cross;
        }

        std::unordered_map<int32, std::vector<uint32>> VerticesBySourcePosition;
        for (uint32 OldIndex = 0; OldIndex < Remap.size(); ++OldIndex)
        {
            if (Remap[OldIndex] == Invalid) continue;

            const int32 PositionIndex = Work.Vertices[OldIndex].SourcePositionIndex;
            if (PositionIndex >= 0)
                VerticesBySourcePosition[PositionIndex].push_back(OldIndex);
        }

        // 원본에서 다른 방향을 향하던 법선은 hard edge로 취급한다.
        // 같은 위치라도 collapse 결과가 벌어졌다면 합치지 않는다.
        constexpr float SmoothNormalCosine = 0.9999f;
        constexpr float SamePositionDistanceSquared = 1e-12f;

        for (const auto& [PositionIndex, OldIndices] : VerticesBySourcePosition)
        {
            if (OldIndices.size() < 2) continue;

            std::vector<bool> Used(OldIndices.size(), false);
            for (size_t I = 0; I < OldIndices.size(); ++I)
            {
                if (Used[I]) continue;

                Used[I] = true;
                const uint32 AnchorOld = OldIndices[I];
                const uint32 AnchorNew = Remap[AnchorOld];
                const FVector AnchorNormal = Work.Vertices[AnchorOld].Vertex.Normal.Normalized();
                const FVector AnchorPosition = Out.Vertices[AnchorNew].Position;

                FVector Sum = NormalSums[AnchorNew];
                std::vector<uint32> Group{AnchorNew};

                for (size_t J = I + 1; J < OldIndices.size(); ++J)
                {
                    if (Used[J]) continue;

                    const uint32 OtherOld = OldIndices[J];
                    const uint32 OtherNew = Remap[OtherOld];
                    const FVector OtherNormal = Work.Vertices[OtherOld].Vertex.Normal.Normalized();
                    const FVector Delta = Out.Vertices[OtherNew].Position - AnchorPosition;

                    if (AnchorNormal.Dot(OtherNormal) < SmoothNormalCosine ||
                        Delta.Dot(Delta) > SamePositionDistanceSquared)
                    {
                        continue;
                    }

                    Used[J] = true;
                    Sum += NormalSums[OtherNew];
                    Group.push_back(OtherNew);
                }

                if (Group.size() > 1)
                {
                    for (uint32 Index : Group)
                        NormalSums[Index] = Sum;
                }
            }
        }

        for (uint32 I = 0;I < static_cast<uint32>( Out.Vertices.Num()); ++I)
        {
            Out.Vertices[I].Normal = NormalSums[I].Normalized();
        }

        FVector Min = Out.Vertices[0].Position;
        FVector Max = Min;

        for (const FVertex& Vertex : Out.Vertices)
        {
            Min.X = std::min(Min.X, Vertex.Position.X);
            Min.Y = std::min(Min.Y, Vertex.Position.Y);
            Min.Z = std::min(Min.Z, Vertex.Position.Z);

            Max.X = std::max(Max.X, Vertex.Position.X);
            Max.Y = std::max(Max.Y, Vertex.Position.Y);
            Max.Z = std::max(Max.Z, Vertex.Position.Z);
        }

        Out.AABB = { Min, Max };

        // LOD1~3을 다시 단순화하지 않는다.
        // 다음 Generate 호출도 항상 LOD0에서 시작한다.
        return Out;
    }
}

FLODGenerateResult FStaticMeshLODGenerator::Generate(const FStaticMeshData& LOD0, TArray<FStaticMeshData>& OutLODs)
{
    OutLODs.Reset();
    FLODGenerateResult Result;

    FString ValidationError;
    if (!LOD0.Validate(ValidationError))
    {
        Result.FailureReason = ValidationError;
        return Result;
    }

    if (LOD0.LODSourceVertices.Num() != LOD0.Vertices.Num())
    {
        Result.FailureReason = "LOD0 has no source topology data";
        return Result;
    }

    const uint32 BaseTriangles = static_cast<uint32>(LOD0.Indices.Num() / 3);
    Result.ActualTriangles[0] = BaseTriangles;

    for (uint32 LOD = 0; LOD < 4; ++LOD)
    {
        Result.TargetTriangles[LOD] = static_cast<uint32>(std::floor(BaseTriangles * TargetRatios[LOD]));
    }

    std::vector<FQuadric> Quadrics;
    if (!BuildVertexQuadrics(LOD0, Quadrics, Result.FailureReason))
    {
        return Result;
    }

    FWorkMesh Work;
    if (!BuildWorkMesh(LOD0, Quadrics, Work, Result.FailureReason))
    {
        return Result;
    }

    bool bReachedAllTargets = true;
    const double BaseVolume = ComputeSignedVolume(LOD0);
    const FVector BaseExtent = LOD0.AABB.Max - LOD0.AABB.Min;
    const double BaseDiagonal = std::sqrt(static_cast<double>(BaseExtent.Dot(BaseExtent)));

    for (uint32 LOD = 1; LOD <= 3; ++LOD)
    {
        // 이전 단계의 Work를 이어 쓰되 살아남은 메시로 보호 상태를 다시 계산한다.
        // 보호 상태가 바뀌므로 각 단계의 후보 힙도 다시 만든다.
        MarkProtectedVertices(LOD0, Work, LOD, Result);

        FCollapseHeap Heap;
        for (uint32 A = 0; A < Work.Vertices.size(); ++A)
        {
            if (!Work.Vertices[A].bAlive) continue;
            for (uint32 B : Work.Vertices[A].Neighbors)
            {
                if (A < B) PushCandidate(A, B, Work, Heap);
            }
        }

        // 빈 메시를 만들지 않도록 적어도 한 삼각형은 남긴다.
        const uint32 StopAt = std::max(1u, Result.TargetTriangles[LOD]);
        const double MaxError = MaxErrorRatios[LOD] * BaseDiagonal;
        const double MaxErrorSquared = MaxError * MaxError;
        while (Work.LiveTriangles > StopAt && !Heap.empty())
        {
            const FCollapseCandidate Candidate = Heap.top();
            Heap.pop();

            const FWorkVertex& VA = Work.Vertices[Candidate.A];
            const FWorkVertex& VB = Work.Vertices[Candidate.B];

            if (!VA.bAlive || !VB.bAlive ||
                VA.Revision != Candidate.RevisionA ||
                VB.Revision != Candidate.RevisionB)
                continue;

            // 이음매 쌍은 반대쪽 쌍둥이 edge도 후보를 만들 때 그대로여야 한다.
            if (Candidate.IsSeamPair())
            {
                const FWorkVertex& VTA = Work.Vertices[Candidate.TwinA];
                const FWorkVertex& VTB = Work.Vertices[Candidate.TwinB];
                if (!VTA.bAlive || !VTB.bAlive ||
                    VTA.Revision != Candidate.RevisionTwinA ||
                    VTB.Revision != Candidate.RevisionTwinB)
                    continue;
            }

            // 힙은 누적 비용 순이라 오차가 큰 후보 뒤에도 작은 후보가 남아 있을 수 있다. 멈추지 않고 건너뛴다.
            if (Candidate.ErrorSquared > MaxErrorSquared)
            {
                ++Result.RejectedError;
                continue;
            }

            const ECollapseCheck Check = CheckCollapse(Candidate, Work);
            if (Check == ECollapseCheck::Topology)
            {
                ++Result.RejectedTopology;
                continue;
            }
            if (Check == ECollapseCheck::Flip)
            {
                ++Result.RejectedFlips;
                continue;
            }

            ApplyCollapse(Candidate, Work, Heap);
        }

        if (Work.LiveTriangles > Result.TargetTriangles[LOD])
            bReachedAllTargets = false;

        FStaticMeshData Snapshot = BuildLODMesh(Work, LOD0);
        FString Error;
        if (!Snapshot.Validate(Error))
        {
            Result.FailureReason = Error;
            OutLODs.Reset();
            return Result;
        }

        Result.ActualTriangles[LOD] = Work.LiveTriangles;
        if (std::abs(BaseVolume) > 1e-12)
            Result.VolumeRatio[LOD] = static_cast<float>(ComputeSignedVolume(Snapshot) / BaseVolume);
        OutLODs.Add(std::move(Snapshot));
    }

    Result.bSuccess = true;
    if (!bReachedAllTargets)
        Result.FailureReason = "Fixed target not reached; best effort LOD used";
    return Result;
}
