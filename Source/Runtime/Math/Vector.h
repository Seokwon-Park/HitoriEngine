#pragma once

#include <iostream>
#include "../Core/Types.h"
#include "MathUtility.h"

struct FVector {

	union
	{
		float V[3];
		struct
		{
			float X;
			float Y;
			float Z;
		};
	};

public:
	/* Constructor */
	FVector();
	FVector(float x, float y, float z);
	explicit FVector(float f);
	FVector(const FVector& V1) = default;	// trivially copyable 유지 (바이너리 저장, memcpy)
	// ~FVector();

public:
	/* Public Functions */
	void Set(float x, float y, float z);

	float SizeSquared() const;
	float Size() const; // 길이 반환
	float Length() const; // ==  size()
	float Dot(const FVector& V1) const;

	float& Component(int32 index);		// 참조자 반환으로 lvalue로 직접 값수정 가능
	float Component(int32 index) const;

	FVector Add(const FVector& V1) const;
	FVector Subtract(const FVector& V1) const;

	FVector Cross(const FVector& V1) const;
	FVector GetAbs() const;
	FVector Normalized() const;
	FVector GetSafeNormal(float Tolerance = SMALL_NUMBER, const FVector& ResultIfZero = ZeroVector) const;

	FVector GetClampedToMaxSize(float MaxSize) const;

	static const FVector ZeroVector;    // (0, 0, 0)
	static const FVector OneVector;     // (1, 1, 1)
	static const FVector BackwardVector;
	static const FVector DownVector;
	static const FVector ForwardVector;
	static const FVector LeftVector;
	static const FVector RightVector;
	static const FVector UpVector;
	static const FVector XAxisVector;
	static const FVector YAxisVector;
	static const FVector ZAxisVector;
	// float GetMax();
	//// float GetMin();
	// float GetAbsMax();
	// float GetAbsMin();

	// GetSafeNormal
	// IsNearlyZero
	// Equals
	// ClampSize


/* operator */

	FVector operator - ();
	FVector operator - () const;
	FVector& operator = (const FVector& V1) = default;	// trivially copyable 유지

	FVector operator - (const FVector& V1) const;
	FVector& operator -= (const FVector& V1);

	FVector operator + (const FVector& V1) const;
	FVector& operator += (const FVector& V1);

	FVector operator * (const FVector& V1) const;
	FVector operator * (const float& f) const;
	FVector& operator *= (const FVector& V1);
	FVector& operator *= (const float& f);

	FVector operator / (const FVector& V1) const;
	FVector operator / (const float& f) const;
	FVector& operator /= (const FVector& V1);
	FVector& operator /= (const float& f);

	FVector operator ^ (const FVector& V1) const;
	float operator | (const FVector& V) const;

	bool operator == (const FVector& V1) const;
	bool operator != (const FVector& V1) const;

	float operator[] (int32 Index) const;
	float& operator[] (int32 Index);

	/* Static */
	static float Dot(const FVector& V1, const FVector& V2);
	static FVector Cross(const FVector& V1, const FVector& V2);
	static float Distance(const FVector& V1, const FVector& V2); // == Dist()
	/*static FVector DegreesToRadians(const FVector& V1);
	static FVector RadiansToDegrees(const FVector& V1);
	static FVector Max(const FVector& V1, const FVector& V2);
	static FVector Max3(const FVector& V1, const FVector& V2, const FVector& V3);
	static FVector Min(const FVector& V1, const FVector& V2);
	static FVector Min3(const FVector& V1, const FVector& V2, const FVector& V3);

	static FVector UnitX();
	static FVector UnitY();
	static FVector UnitZ();

	static FVector Zero();*/

	FVector ProjectOnToNormal(const FVector& Normal) const;
	static FVector VectorPlaneProject(const FVector& V, const FVector& PlaneNormal);
	static bool Coincident(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL);
};

/* Global Operator */
template<typename T>
std::ostream& operator<<(std::ostream& OS, const FVector& V);

inline const FVector FVector::ZeroVector{ 0.f, 0.f, 0.f };
inline const FVector FVector::OneVector{ 1.f, 1.f, 1.f };
inline const FVector FVector::BackwardVector = FVector(-1.0f, 0.0f, 0.0f);
inline const FVector FVector::DownVector = FVector(0.0f, 0.0f, -1.0f);
inline const FVector FVector::ForwardVector = FVector(1.0f, 0.0f, 0.0f);
inline const FVector FVector::LeftVector = FVector(0.0f, -1.0f, 0.0f);
inline const FVector FVector::RightVector = FVector(0.0f, 1.0f, 0.0f);
inline const FVector FVector::UpVector = FVector(0.0f, 0.0f, 1.0f);
inline const FVector FVector::XAxisVector = FVector(1.0f, 0.0f, 0.0f);
inline const FVector FVector::YAxisVector = FVector(0.0f, 1.0f, 0.0f);
inline const FVector FVector::ZAxisVector = FVector(0.0f, 0.0f, 1.0f);
