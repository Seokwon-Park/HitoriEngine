#include "EnginePCH.h"

#include "Vector4.h"
#include "EngineMath.h"
#include "Color.h"

#include <cmath>
#include <assert.h>

FVector4::FVector4()
{
	X = 0.0f;
	Y = 0.0f;
	Z = 0.0f;
	W = 0.0f;
}

FVector4::FVector4(float x, float y, float z, float w)
{
	X = x;
	Y = y;
	Z = z;
	W = w;
}

FVector4::FVector4(float f)
{
	X = f;
	Y = f;
	Z = f;
	W = f;
}

FVector4::FVector4(const FVector& V1, float w)
{
	X = V1.X;
	Y = V1.Y;
	Z = V1.Z;
	W = w;
}

FVector4::FVector4(const FLinearColor& C)
{
	X = C.R;
	Y = C.G;
	Z = C.B;
	W = C.A;
}

/* Functions */

void FVector4::Set(float x, float y, float z, float w)
{
	X = x;
	Y = y;
	Z = z;
	W = w;
}

void FVector4::Set(const FVector& V1, float w)
{
	X = V1.X;
	Y = V1.Y;
	Z = V1.Z;
	W = w;
}

void FVector4::Set(const FVector4& V1)
{
	X = V1.X;
	Y = V1.Y;
	Z = V1.Z;
	W = V1.W;
}

float FVector4::Size()
{
	float sum = X * X + Y * Y + Z * Z + W * W;
	return sqrt(sum);
}

float FVector4::Length()
{
	float sum = X * X + Y * Y + Z * Z + W * W;
	return sqrt(sum);
}

FVector4 FVector4::Normalized()
{
	float size = Size();
	
	if (!FMath::IsNearlyZero(size))
	{
		X /= size;
		Y /= size;
		Z /= size;
		W /= size;
	}

	return* this;
}

float& FVector4::Component(int32 index)
{
	assert(index >= 0 && index <= 2);
	if (index == 0) return X;
	if (index == 1) return Y;
	if (index == 2) return Z;
	if (index == 3) return W;
	return X;					// 예외의 경우에서 X를 반환 (임시)
}

float FVector4::Component(int32 index) const
{
	assert(index >= 0 && index <= 2);
	if (index == 0) return X;
	if (index == 1) return Y;
	if (index == 2) return Z;
	if (index == 3) return W;
	return 0.0f;				// 이외의 경우에서 0을 반환 (임시)
}

float FVector4::Dot(const FVector4& V1) const
{
	return X * V1.X + Y * V1.Y + Z * V1.Z + W * V1.W;
}

FVector4 FVector4::Cross(const FVector4& V1) const
{
	return FVector4(Y * V1.Z - Z * V1.Y, Z * V1.X - X * V1.Z, X * V1.Y - Y * V1.X, 0);
}


FVector4 FVector4::GetAbs() {
	float AbsX = X;
	float AbsY = Y;
	float AbsZ = Z;
	float AbsW = W;
	if (X < 0) AbsX = -X;
	if (Y < 0) AbsY = -Y;
	if (Z < 0) AbsZ = -Z;
	if (W < 0) AbsW = -W;
	return FVector4(AbsX, AbsY, AbsZ, AbsW);
}

/* Operator */

FVector4 FVector4::operator - ()
{
	return FVector4(-X, -Y, -Z, -W);
}

FVector4 FVector4::operator - (const FVector4& V1) const
{
	return FVector4(X - V1.X, Y - V1.Y, Z - V1.Z, W - V1.W);
}

FVector4& FVector4::operator -= (const FVector4& V1)
{
	X -= V1.X;
	Y -= V1.Y;
	Z -= V1.Z;
	W -= V1.W;
	return *this;
}

FVector4 FVector4::operator + (const FVector4& V1) const
{
	return FVector4(X + V1.X, Y + V1.Y, Z + V1.Z, W + V1.W);
}

FVector4& FVector4::operator += (const FVector4& V1)
{
	X += V1.X;
	Y += V1.Y;
	Z += V1.Z;
	W += V1.W;
	return *this;
}

FVector4 FVector4::operator * (const FVector4& V1) const
{
	return FVector4(X * V1.X, Y * V1.Y, Z * V1.Z, W * V1.W);
}

FVector4 FVector4::operator * (const float& f) const
{
	return FVector4(X * f, Y * f, Z * f, W * f);
}

FVector4 FVector4::operator * (const FMatrix& M) const
{

	return M.TransformFVector4(*this);
	/*return FVector4(
		X * M[0][0] + Y * M[1][0] + Z * M[2][0] + W * M[3][0],
		X * M[0][1] + Y * M[1][1] + Z * M[2][1] + W * M[3][1],
		X * M[0][2] + Y * M[1][2] + Z * M[2][2] + W * M[3][2],
		X * M[0][3] + Y * M[1][3] + Z * M[2][3] + W * M[3][3]
	);*/
}

FVector4& FVector4::operator *= (const FVector4& V1)
{
	X *= V1.X;
	Y *= V1.Y;
	Z *= V1.Z;
	W *= V1.W;
	return *this;
}

FVector4& FVector4::operator *= (const float& f)
{
	X *= f;
	Y *= f;
	Z *= f;
	W *= f;
	return *this;
}

FVector4 FVector4::operator / (const FVector4& V1) const
{
	assert(V1.X != 0 && V1.Y != 0 && V1.Z != 0 && V1.W != 0);
	return FVector4(X / V1.X, Y / V1.Y, Z / V1.Z, W / V1.W);
}

FVector4 FVector4::operator / (const float& f) const
{
	return FVector4(X / f, Y / f, Z / f, W / f);
}

FVector4& FVector4::operator /= (const FVector4& V1)
{
	X /= V1.X;
	Y /= V1.Y;
	Z /= V1.Z;
	W /= V1.W;
	return *this;
}

FVector4& FVector4::operator /= (const float& f)
{
	X /= f;
	Y /= f;
	Z /= f;
	W /= f;
	return *this;
}

// 스트림 출력 연산자와 함께 사용할 시 괄호로 묶을 것
FVector4 FVector4::operator ^ (const FVector4& V1) const
{
	return Cross(V1);
}

bool FVector4::operator == (const FVector4& V1) const
{
	return (X == V1.X) && (Y == V1.Y) && (Z == V1.Z) && (W == V1.W);
}

bool FVector4::operator != (const FVector4& V1) const
{
	return !(*this == V1);
}

float FVector4::operator[] (int32 Index) const
{
	return V[Index];
}

float& FVector4::operator[] (int32 Index)
{
	return V[Index];
}

/* Global Operator */
std::ostream& operator << (std::ostream& OS, const FVector4& V)
{
	OS << "(" << V.X << ", " << V.Y << ", " << V.Z << ", " << V.W << ")";
	return OS;
}


/* Static Functions */

float FVector4::DotProduct(const FVector4& V1, const FVector4& V2)
{
	return V1.Dot(V2);
}

FVector4 FVector4::CrossProduct(const FVector4& V1, const FVector4& V2)
{
	return V1.Cross(V2);
}

float FVector4::Distance(const FVector4& V1, const FVector4& V2)
{
	float dX = V1.X - V2.X;
	float dY = V1.Y - V2.Y;
	float dZ = V1.Z - V2.Z;
	float dW = V1.W - V2.W;
	float sum = dX * dX + dY * dY + dZ * dZ + dW * dW;

	return sqrt(sum);
}
