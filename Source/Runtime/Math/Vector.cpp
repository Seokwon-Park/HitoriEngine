#include "EnginePCH.h"
#include "Math/Vector.h"
#include <cmath>
#include <assert.h>

FVector::FVector()
{
	X = 0;
	Y = 0;
	Z = 0;
}

FVector::FVector(float x, float y, float z)
{
	X = x;
	Y = y;
	Z = z;
}

FVector::FVector(float f)
{
	X = f;
	Y = f;
	Z = f;
}

/* Functions */

void FVector::Set(float x, float y, float z)
{
	X = x;
	Y = y;
	Z = z;
}


float FVector::SizeSquared() const
{
	return X * X + Y * Y + Z * Z;
}

float FVector::Size() const
{
	float sum = X * X + Y * Y + Z * Z;
	return sqrt(sum);
}


float FVector::Length() const
{
	float sum = X * X + Y * Y + Z * Z;
	return sqrt(sum);
}

FVector FVector::Normalized() const
{
	float size = Size();

	if (!FMath::IsNearlyZero(size))
	{
		return FVector(X / size, Y / size, Z / size);
	}

	return *this;
}

FVector FVector::GetSafeNormal(float Tolerance, const FVector& ResultIfZero) const
{
	const float SquareSum = X * X + Y * Y + Z * Z;

	if (SquareSum == 1.0f)
	{
		return *this;
	}
	else if (SquareSum < Tolerance)
	{
		return ResultIfZero;
	}

	const float InvSize = 1.0f / sqrtf(SquareSum);

	return FVector(X * InvSize, Y * InvSize, Z * InvSize);
}

FVector FVector::GetClampedToMaxSize(float MaxSize) const
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return ZeroVector;
	}

	const float VSq = SizeSquared();
	if (VSq > MaxSize * MaxSize)
	{
		const float Scale = MaxSize / sqrtf(VSq);
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}
	else
	{
		return *this;
	}	
}

float& FVector::Component(int32 index)
{
	assert(index >= 0 && index <= 2);
	if (index == 0) return X;
	if (index == 1) return Y;
	if (index == 2) return Z;
	return X;					// 예외의 경우에서 X를 반환 (임시)
}

float FVector::Component(int32 index) const
{
	assert(index >= 0 && index <= 2);
	if (index == 0) return X;
	if (index == 1) return Y;
	if (index == 2) return Z;
	return 0.0f;				// 이외의 경우에서 0을 반환 (임시)
}

FVector FVector::Add(const FVector& V1) const
{
	return FVector(X + V1.X, Y + V1.Y, Z + V1.Z);
}

FVector FVector::Subtract(const FVector& V1) const
{
	return FVector(X - V1.X, Y - V1.Y, Z - V1.Z);
}

float FVector::Dot(const FVector& V1) const
{
	return X * V1.X + Y * V1.Y + Z * V1.Z;
}

FVector FVector::Cross(const FVector& V1) const
{
	return FVector(Y * V1.Z - Z * V1.Y, Z * V1.X - X * V1.Z, X * V1.Y - Y * V1.X);
}

FVector FVector::GetAbs() const {
	float AbsX = X;
	float AbsY = Y;
	float AbsZ = Z;
	if (X < 0) AbsX = -X;
	if (Y < 0) AbsY = -Y;
	if (Z < 0) AbsZ = -Z;
	return FVector(AbsX, AbsY, AbsZ);
}

/* Operator */

FVector FVector::operator - ()
{
	return FVector(-X, -Y, -Z);
}

FVector FVector::operator-() const
{
	return FVector(-X, -Y, -Z);
}


FVector FVector::operator - (const FVector& V1) const
{
	return FVector(X - V1.X, Y - V1.Y, Z - V1.Z);
}

FVector& FVector::operator -= (const FVector& V1)
{
	X -= V1.X;
	Y -= V1.Y;
	Z -= V1.Z;
	return *this;
}

FVector FVector::operator + (const FVector& V1) const
{
	return FVector(X + V1.X, Y + V1.Y, Z + V1.Z);
}

FVector& FVector::operator += (const FVector& V1)
{
	X += V1.X;
	Y += V1.Y;
	Z += V1.Z;
	return *this;
}

FVector FVector::operator * (const FVector& V1) const
{
	return FVector(X * V1.X, Y * V1.Y, Z * V1.Z);
}

FVector& FVector::operator *= (const FVector& V1)
{
	X *= V1.X;
	Y *= V1.Y;
	Z *= V1.Z;
	return *this;
}

FVector& FVector::operator *= (const float& f)
{
	X *= f;
	Y *= f;
	Z *= f;
	return *this;
}

FVector FVector::operator * (const float& f) const
{
	return FVector(X * f, Y * f, Z * f);
}

FVector FVector::operator / (const FVector& V1) const
{
	assert(V1.X != 0 || V1.Y != 0 || V1.Z != 0);
	return FVector(X / V1.X, Y / V1.Y, Z / V1.Z);
}

FVector FVector ::operator / (const float& f) const
{
	return FVector(X / f, Y / f, Z / f);
}

FVector& FVector::operator /= (const FVector& V1)
{
	const float InvX = 1.0f / V1.X;
	const float InvY = 1.0f / V1.Y;
	const float InvZ = 1.0f / V1.Z;

	X *= InvX;
	Y *= InvY;
	Z *= InvZ;

	return *this;
}

FVector& FVector::operator /= (const float& f)
{
	const float InvF = 1.0f / f;
	X *= InvF;
	Y *= InvF;
	Z *= InvF;

	return *this;
}

// 스트림 출력 연산자와 함께 사용할 시 괄호로 묶을 것
FVector FVector::operator ^ (const FVector& V1) const
{
	return this->Cross(V1);
}

float FVector::operator|(const FVector& V) const
{
	return this->Dot(V);
}

bool FVector::operator == (const FVector& V1) const
{
	return (X == V1.X) && (Y == V1.Y) && (Z == V1.Z);
}

bool FVector::operator != (const FVector& V1) const
{
	return !(*this == V1);
}

float FVector::operator[] (int32 Index) const
{
	return V[Index];
}

float& FVector::operator[] (int32 Index)
{
	return V[Index];
}

/* Global Operator */
std::ostream& operator << (std::ostream& OS, const FVector& V)
{
	OS << "(" << V.X << ", " << V.Y << ", " << V.Z << ")";
	return OS;
}


/* Static Functions */
float FVector::Dot(const FVector& V1, const FVector& V2)
{
	return V1.Dot(V2);
}

FVector FVector::Cross(const FVector& V1, const FVector& V2)
{
	return V1.Cross(V2);
}

float FVector::Distance(const FVector& V1, const FVector& V2)
{
	float dX = V1.X - V2.X;
	float dY = V1.Y - V2.Y;
	float dZ = V1.Z - V2.Z;
	float sum = dX * dX + dY * dY + dZ * dZ;

	return sqrt(sum);
}

FVector FVector::ProjectOnToNormal(const FVector& Normal) const
{
	return (Normal * (*this | Normal));
}

FVector FVector::VectorPlaneProject(const FVector& V, const FVector& PlaneNormal)
{
	return V - V.ProjectOnToNormal(PlaneNormal);
}

bool FVector::Coincident(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold)
{
	const float NormalDot = Normal1 | Normal2;
	return NormalDot >= ParallelCosineThreshold;
}
