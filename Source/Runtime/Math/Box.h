#pragma once

struct FBox
{
	FVector Min;
	FVector Max;
	

	FBox GetWorldAABB(const FMatrix& M) const
	{
		FVectorRegister Minreg = VectorSIMD::LoadFloat3(&Min.X);
		FVectorRegister Maxreg = VectorSIMD::LoadFloat3(&Max.X);
		FVectorRegister Centerreg = VectorSIMD::Mul(VectorSIMD::Add(Minreg, Maxreg), VectorSIMD::SetVal(0.5f));
		FVectorRegister Extentreg = VectorSIMD::Mul(VectorSIMD::Sub(Maxreg, Minreg), VectorSIMD::SetVal(0.5f));

		// 중심은 그냥 변환
		float center[4];
		VectorSIMD::Store(center, Centerreg);
		FVector4 C = FVector4(center[0], center[1], center[2], 1.0f) * M;

		// 범위는 회전 부분의 절댓값으로 변환
		FVectorRegister Ereg = VectorSIMD::Add(
			VectorSIMD::Add(
				VectorSIMD::Mul(VectorSIMD::SplatX(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[0]))),
				VectorSIMD::Mul(VectorSIMD::SplatY(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[1])))
			),
			VectorSIMD::Mul(VectorSIMD::SplatZ(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[2])))
		);

		FVector E;
		VectorSIMD::StoreFloat3(&E.X, Ereg);

		return FBox{ FVector(C.X, C.Y, C.Z) - E, FVector(C.X, C.Y, C.Z) + E };
	}

	void Expand(const FBox& Other)
	{
		Min.X = std::min(Min.X, Other.Min.X);
		Min.Y = std::min(Min.Y, Other.Min.Y);
		Min.Z = std::min(Min.Z, Other.Min.Z);

		Max.X = std::max(Max.X, Other.Max.X);
		Max.Y = std::max(Max.Y, Other.Max.Y);
		Max.Z = std::max(Max.Z, Other.Max.Z);
	}
};
