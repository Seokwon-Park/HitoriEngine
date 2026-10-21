#pragma once

#include <iostream>
#include "../Core/Types.h"


//template<typename T>
//struct TVector;
struct FVector;
struct FMatrix;
struct FLinearColor;

struct FVector4 {

	union
	{
		float V[4];
		struct
		{
			float X;
			float Y;
			float Z;
			float W;
		};
	};

	/* Constructor */
	FVector4();
	FVector4(float x, float y, float z, float w);
	explicit FVector4(float f);
	FVector4(const FVector4& V1) = default;	// trivially copyable 유지 (바이너리 저장, memcpy)
	FVector4(const FVector& V1, float w = 0.0f);
	explicit FVector4(const FLinearColor& C);
	// ~FVector();

public:
	/* Public Functions */
	void Set(float x, float y, float z, float w);
	void Set(const FVector& V1, float w = 0.0f);
	void Set(const FVector4& V1);

	float& Component(int32 index);		// 참조자 반환으로 lvalue로 직접 값수정 가능
	float Component(int32 index) const;
	float Size(); // 길이 반환
	float Length(); // ==  size()
	float Dot(const FVector4& V1) const;

	FVector4 Normalized();
	FVector4 Cross(const FVector4& V1) const;
	FVector4 GetAbs();

	// float GetMax();
	//// float GetMin();
	// float GetAbsMax();
	// float GetAbsMin();

	// GetSafeNormal

	// ClampSize


/* operator */

	FVector4 operator - ();
	FVector4& operator = (const FVector4& V1) = default;	// trivially copyable 유지

	FVector4 operator - (const FVector4& V1) const;
	FVector4& operator -= (const FVector4& V1);

	FVector4 operator + (const FVector4& V1) const;
	FVector4& operator += (const FVector4& V1);

	FVector4 operator * (const FVector4& V1) const;
	FVector4 operator * (const float& f) const;
	FVector4 operator * (const FMatrix& M) const;

	FVector4& operator *= (const FVector4& V1);
	FVector4& operator *= (const float& f);

	FVector4 operator / (const FVector4& V1) const;
	FVector4 operator / (const float& f) const;
	FVector4& operator /= (const FVector4& V1);
	FVector4& operator /= (const float& f);

	FVector4 operator ^ (const FVector4& V1) const;

	bool operator == (const FVector4& V1) const;
	bool operator != (const FVector4& V1) const;

	float operator[] (int32 Index) const;
	float& operator[] (int32 Index);


	static float DotProduct(const FVector4& V1, const FVector4& V2);
	static FVector4 CrossProduct(const FVector4& V1, const FVector4& V2);
	static float Distance(const FVector4& V1, const FVector4& V2); // == Dist()
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

};

/* Global Operator */
std::ostream& operator<<(std::ostream& OS, const FVector4& V);