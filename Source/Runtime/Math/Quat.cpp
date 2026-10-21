#include "EnginePCH.h"
#include "Quat.h"
#include "Rotator.h"
#include "EngineMath.h"

FQuat::FQuat()
{
	X = 0.0f; Y = 0.0f; Z = 0.0f; W = 1.0f;
}

FQuat::FQuat(float InX, float InY, float InZ, float InW)
{
	X = InX; Y = InY; Z = InZ; W = InW;
}

FQuat::FQuat(FRotator InRoatator)
{
	// TODO
}

/* Functions */

FQuat FQuat::operator * (const FQuat& Other) const
{
	FQuatRegister A = VectorSIMD::Load(V);
	FQuatRegister B = VectorSIMD::Load(Other.V);

	// i 성분: A0 * B3 + A3 * B0 + A1 * B2 - A2 * B1
	FQuatRegister IA = VectorSIMD::Shuffle<0, 3, 1, 2>(A, A);
	FQuatRegister IB = VectorSIMD::Shuffle<3, 0, 2, 1>(B, B);

	// j 성분: A1 * B3 + A3 * B1 + A2 * B0 - A0 * B2
	FQuatRegister JA = VectorSIMD::Shuffle<1, 3, 2, 0>(A, A);
	FQuatRegister JB = VectorSIMD::Shuffle<3, 1, 0, 2>(B, B);

	// k 성분: A3 * B2 + A2 * B3 + A0 * B1 - A1 * B0
	FQuatRegister KA = VectorSIMD::Shuffle<3, 2, 0, 1>(A, A);
	FQuatRegister KB = VectorSIMD::Shuffle<2, 3, 1, 0>(B, B);

	// R 성분: -( A0 * B0 + A1 * B1 + A2 * B2 - A3 * B3 )

	FQuatRegister Sign = VectorSIMD::SetVal(1.0f, 1.0f, 1.0f, -1.0f);

	FQuatRegister I = VectorSIMD::Mul(IA, IB);
	FQuatRegister J = VectorSIMD::Mul(JA, JB);
	FQuatRegister K = VectorSIMD::Mul(KA, KB);
	FQuatRegister R = VectorSIMD::Mul(A, B);

	float SI = VectorSIMD::Dot(I, Sign);
	float SJ = VectorSIMD::Dot(J, Sign);
	float SK = VectorSIMD::Dot(K, Sign);
	float SR = -VectorSIMD::Dot(R, Sign);

	return FQuat(SI, SJ, SK, SR);

}

// 켤레 복소수
FQuat FQuat::Conjugate() const
{
	FQuatRegister Reg = VectorSIMD::Load(V);
	FQuatRegister Sign = VectorSIMD::SetVal(-1.0f, -1.0f, -1.0f, 1.0f);
	FQuat Result;
	VectorSIMD::Store(Result.V, VectorSIMD::Mul(Reg, Sign));
	return Result;
}

// 단위 쿼터니언 전용 역
FQuat FQuat::UnitInverse() const
{
	return Conjugate();
}

FQuat FQuat::Inverse() const
{
	FQuatRegister Reg = VectorSIMD::Load(V);
	FQuatRegister Sign = VectorSIMD::SetVal(-1.0f, -1.0f, -1.0f, 1.0f);

	// Conjugate
	FQuatRegister Conjugate = VectorSIMD::Mul(Reg, Sign);
	FQuatRegister LengthSq = VectorSIMD::SetVal(VectorSIMD::LengthSquared(Reg));
	FQuatRegister ResultReg = VectorSIMD::Div(Conjugate, LengthSq);

	FQuat Result;
	VectorSIMD::Store(Result.V, ResultReg);

	return Result;
}

FQuat FQuat::Normalized() const
{
	FQuatRegister Reg = VectorSIMD::Load(V);
	FQuat Result;
	VectorSIMD::Store(Result.V, VectorSIMD::Normalized(Reg));
	return Result;
}

FVector FQuat::RotateVector(const FVector& V) const
{
	FQuatRegister Q = VectorSIMD::Load(this->V);
	FVectorRegister Vec = VectorSIMD::LoadFloat3(V.V);

	// Q.xyz
	FVectorRegister Qv = Qv = VectorSIMD::SetWZero(Q);

	// T = 2 * cross(Q.xyz, V)
	FVectorRegister T = VectorSIMD::Mul( VectorSIMD::SetVal(2.0f), VectorSIMD::Cross3(Qv, Vec));

	// V + W*T
	FVectorRegister Result = VectorSIMD::Add( Vec, VectorSIMD::Mul( VectorSIMD::SplatW(Q), T ));

	// + cross(Q.xyz, T)
	Result = VectorSIMD::Add( Result, VectorSIMD::Cross3(Qv, T));

	FVector Out;
	VectorSIMD::StoreFloat3(Out.V, Result);

	return Out;
}

FMatrix FQuat::ToFMatrix() const
{
	const float XX = X * X;
	const float YY = Y * Y;
	const float ZZ = Z * Z;

	const float XY = X * Y;
	const float XZ = X * Z;
	const float YZ = Y * Z;

	const float WX = W * X;
	const float WY = W * Y;
	const float WZ = W * Z;

	return FMatrix(
		1.0f - 2.0f * (YY + ZZ), 2.0f * (XY + WZ), 2.0f * (XZ - WY), 0.0f,
		2.0f * (XY - WZ), 1.0f - 2.0f * (XX + ZZ), 2.0f * (YZ + WX), 0.0f,
		2.0f * (XZ + WY) , 2.0f * (YZ - WX), 1.0f - 2.0f * (XX + YY), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

FRotator FQuat::ToFRotator() const
{
	FRotator ResultRotator;

	// Pitch
	const float SinPitch = 2.0f * (W * Y - Z * X);

	// Gimbal Lock 방지를 위해 [-1, 1] 범위로 Clamp
	const float ClampedSinPitch = FMath::Clamp(SinPitch, -1.0f, 1.0f);

	ResultRotator.Pitch = asinf(ClampedSinPitch);

	// Roll
	ResultRotator.Roll = atan2f( 2.0f * (W * X + Y * Z), 1.0f - 2.0f * (X * X + Y * Y));

	// Yaw
	ResultRotator.Yaw = atan2f( 2.0f * (W * Z + X * Y), 1.0f - 2.0f * (Y * Y + Z * Z));

	// Radian -> Degree
	ResultRotator.Pitch = FMath::RadiansToDegrees(ResultRotator.Pitch);
	ResultRotator.Yaw = FMath::RadiansToDegrees(ResultRotator.Yaw);
	ResultRotator.Roll = FMath::RadiansToDegrees(ResultRotator.Roll);

	return ResultRotator;
}

FVector FQuat::GetForwardVector() const
{
	return FVector(
		1.0f - 2.0f * (Y * Y + Z * Z),
		2.0f * (X * Y + W * Z),
		2.0f * (X * Z - W * Y)
	);
}

FVector FQuat::GetRightVector() const
{
	return FVector(
		2.0f * (X * Y - W * Z),
		1.0f - 2.0f * (X * X + Z * Z),
		2.0f * (Y * Z + W * X)
	);
}

FVector FQuat::GetUpVector() const
{
	return FVector(
		2.0f * (X * Z + W * Y),
		2.0f * (Y * Z - W * X),
		1.0f - 2.0f * (X * X + Y * Y)
	);
}

/* Statics */

FQuat FQuat::Identity()
{
	return FQuat(0, 0, 0, 1.0f);
}

FQuat FQuat::MakeFromAxisAngle(const FVector& Axis, float AngleRadians)
{
	FVector NormalizeddAxis = Axis.Normalized();

	const float HalfAngle = AngleRadians * 0.5f;

	const float S = sinf(HalfAngle);
	const float C = cosf(HalfAngle);

	return FQuat(
		NormalizeddAxis.X * S,
		NormalizeddAxis.Y * S,
		NormalizeddAxis.Z * S,
		C
	);
}

FQuat FQuat::MakeFromEuler(float RollRadian, float PitchRadian, float YawRadian)
{
	const float HalfPitch = PitchRadian * 0.5f;
	const float HalfYaw = YawRadian * 0.5f;
	const float HalfRoll = RollRadian * 0.5f;

	const float SP = sinf(HalfPitch);
	const float CP = cosf(HalfPitch);

	const float SY = sinf(HalfYaw);
	const float CY = cosf(HalfYaw);

	const float SR = sinf(HalfRoll);
	const float CR = cosf(HalfRoll);

	return FQuat(
		// X
		SR * CP * CY - CR * SP * SY,

		// Y
		CR * SP * CY + SR * CP * SY,

		// Z
		CR * CP * SY - SR * SP * CY,

		// W
		CR * CP * CY + SR * SP * SY
	);
}
