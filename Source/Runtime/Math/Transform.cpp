#include "EnginePCH.h"
#include "Transform.h"

FTransform::FTransform(FVector InL, FVector InS, FRotator InR)
{
	Location = InL;
	Scale = InS;
	Rotation = InR;
}

FMatrix FTransform::GetLocalMatrix() const
{
    FMatrix RotationMatrix = Rotation.Quaternion().ToFMatrix();

    FMatrix WorldMatrix = FMatrix(
        RotationMatrix[0][0] * Scale.X, RotationMatrix[0][1] * Scale.X, RotationMatrix[0][2] * Scale.X, 0.0f,
        RotationMatrix[1][0] * Scale.Y, RotationMatrix[1][1] * Scale.Y, RotationMatrix[1][2] * Scale.Y, 0.0f,
        RotationMatrix[2][0] * Scale.Z, RotationMatrix[2][1] * Scale.Z, RotationMatrix[2][2] * Scale.Z, 0.0f,
        Location.X, Location.Y, Location.Z, 1.0f
    );

    return WorldMatrix;
}

FVector FTransform::GetForward() const
{
    FQuat Quat = GetOrientation();
    return Quat.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FVector FTransform::GetUp() const
{
    FQuat Quat = GetOrientation();
    return Quat.RotateVector(FVector(0.0f, 0.0f, 1.0f));
}

FVector FTransform::GetRight() const
{
    return GetOrientation().RotateVector(FVector(0.0f, 1.0f, 0.0f));
}

FQuat FTransform::GetOrientation() const
{
    return FQuat::MakeFromEuler(FMath::DegreesToRadians(Rotation.Roll), FMath::DegreesToRadians(Rotation.Pitch), FMath::DegreesToRadians(Rotation.Yaw));
}

FTransform FTransform::Identity = FTransform(FVector(0, 0, 0), FVector(1, 1, 1), FRotator(0, 0, 0));