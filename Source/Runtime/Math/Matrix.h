#pragma once
#include "../Core/Types.h"

//template<typename T>
//struct TVector;
struct FVector;
struct FVector4;
struct FRotator;

// Row Vector sys
struct FMatrix {
	float M[4][4];

public:
	FMatrix();
	FMatrix(const FVector4& InX, const FVector4& InY, const FVector4& InZ, const FVector4& InW);
	FMatrix(
		const float& f00, const float& f01, const float& f02, const float& f03,
		const float& f10, const float& f11, const float& f12, const float& f13,
		const float& f20, const float& f21, const float& f22, const float& f23,
		const float& f30, const float& f31, const float& f32, const float& f33
	);

public:
	void SetAxes(const FVector4& Axis0, const FVector4& Axis1, const FVector4& Axis2, const FVector4& Axis3);
	void SetAxis(int32 i, const FVector& Axis);
	void GetUnitAxis(FVector4& X, FVector4& Y, FVector4& Z) const;

	//XMMATRIX FMatrixToXMMatrix() const;

	FVector4 GetColumn(int32 i) const;
	FVector4 GetOrigin();
	FVector4 GetScaledAxis(FVector4& X, FVector4& Y, FVector4& Z) const;
	FVector4 TransformFVector4(const FVector4& V) const;	
	FVector4 TransformFVector4NoScale(const FVector4& V) const;
	FVector TransformPosition(const FVector& V) const;
	FVector TransformVector(const FVector& V) const;
	FVector TransformVectorNoScale(const FVector& V) const;

	FVector4 InverseTransformPosition(const FVector& V) const;

	FMatrix ApplyScale(float Scale) const;
	FMatrix GetTransposed() const;
	FMatrix Inverse() const;

	FMatrix Multiply(const FMatrix& Other) const;

	float Determinant() const;

	void SetColumn(int32 i, FVector4 Value);
	void SetIdentity();
	void SetOrigin(const FVector& NewOrigin);

	static FMatrix MakeTranslation(const FVector& T);

	//void To3x4MatrixTranspose(float* Out) const;
	// FMatrix TransposeAdjoint();

	//GetFrustum Planes Function// 

	/* Statics */

	//static XMMATRIX FMatrixToXMMatrix(const FMatrix& M);
	//static FMatrix XMMatrixToFMatrix(const XMMATRIX& Matrix);

	static FMatrix MakeView(const FVector& Position, const FRotator& Rotation);
	static FMatrix MakePerspectiveLH(float FOV, float AspectRatio, float Near, float Far);
	static FMatrix MakeOrthographLH(float Width, float Height, float Near, float Far);

/* Operator */

	FMatrix& operator = (const FMatrix& Other);
	FMatrix operator - ();

	const float* operator[] (int32 Index) const;
	float* operator[] (int32 Index);

	bool operator != (const FMatrix& Other) const;
	bool operator == (const FMatrix& Other) const;

	FMatrix operator * (const FMatrix& Other) const;
	FMatrix operator * (const float& Other) const;
	FVector4 operator * (const FVector4& Other) const;

	FMatrix& operator *= (const FMatrix& Other);
	FMatrix& operator *= (float Other);

	FMatrix operator + (const FMatrix& Other) const;
	FMatrix& operator += (const FMatrix& Other);
	FMatrix operator - (const FMatrix& Other) const;
	FMatrix& operator -= (const FMatrix& Other);

	static FMatrix Identity;
};



/* Global Operator*/
std::ostream& operator << (std::ostream& OS, const FMatrix& M);

FRotator MatrixToRotator(const FMatrix& M);

