#pragma once

#include "../Core/Types.h"

struct FVector2
{
	union
	{
		float V[2];
		struct
		{
			float X;
			float Y;

		};
	};

	FVector2()
		:X(0), Y(0){}
	FVector2(float InX, float InY)
		:X(InX), Y(InY) {
	}
	explicit FVector2(float InValue)
		:FVector2(InValue, InValue) {
	}

	float& operator[](uint64 _index) { return V[_index]; }
	const float& operator[](uint64 _index) const { return V[_index]; }

	FVector2 operator-(const FVector2& Other) { return FVector2(X - Other.X, Y - Other.Y); }
	FVector2 operator-(const FVector2& Other) const { return FVector2(X - Other.X, Y - Other.Y); }

	FVector2 operator+(const FVector2& Other) { return FVector2(X + Other.X, Y + Other.Y); }
	FVector2 operator+(const FVector2& Other) const { return FVector2(X + Other.X, Y + Other.Y); }

	FVector2 operator*(const float Scalar) { return FVector2(X * Scalar, Y * Scalar); }
	FVector2 operator*(const float Scalar) const { return FVector2(X * Scalar, Y * Scalar); }

	bool operator==(const FVector2& Other) const { return X == Other.X && Y == Other.Y; }
	bool operator!=(const FVector2& Other) const { return !(*this == Other); }
};