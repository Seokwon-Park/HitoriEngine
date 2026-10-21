#pragma once
#include "Math/EngineMath.h"

// Degree but Quat: Radians
struct FRotator
{
	union
	{
		float V[3];
		struct
		{
			float Pitch;
			float Yaw;
			float Roll;
		};
	};
	FRotator();
	FRotator(float P, float Y, float R);
	FRotator(FVector V);

	static FRotator Identitiy;

	FQuat Quaternion() const;

	float operator[] (int32 Index) const
	{
		return V[Index];
	}

	float& operator[] (int32 Index)
	{
		return V[Index];
	}

	FRotator operator+(const FRotator& Other)
	{
		return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
	}

	FRotator operator*(const float Scalar)
	{
		return FRotator(Pitch * Scalar, Yaw * Scalar, Roll * Scalar);
	}
};

FRotator operator+(FRotator Rot, const FVector& Vec);



