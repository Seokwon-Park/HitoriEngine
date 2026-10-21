#pragma once

#include "Matrix.h"
#include "VectorRegister.h"

struct FMatrixRegister 
{
	FVectorRegister R[4];
public:
	FMatrixRegister();
	FMatrixRegister(FVectorRegister R0, FVectorRegister R1, FVectorRegister R2, FVectorRegister R3);

	FMatrix ToFMatrix() const;
	FMatrixRegister Transpose() const;
	
	float Determinant3x3(FVectorRegister A, FVectorRegister B, FVectorRegister C) const;

	// 행렬식
	float Determinant() const;
	// 역행렬
	FMatrixRegister Inverse() const;

	static FMatrixRegister Load(const FMatrix& M);

	inline static FMatrixRegister Identity();

};