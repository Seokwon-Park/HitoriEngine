#pragma once
#include <immintrin.h>


#if defined(__x86_64__) || defined(_M_X64)
// 인텔/AMD 환경
using FVectorRegister = __m128;
#else
struct alignas(16) FVectorRegister { float v[4]; };
#endif

namespace VectorSIMD
{
	// SSE 구현
	inline FVectorRegister Load(const float* Ptr) { return _mm_loadu_ps(Ptr); }
	inline void Store(float* Ptr, FVectorRegister V) { _mm_storeu_ps(Ptr, V); }


	inline FVectorRegister LoadFloat3(const float* Ptr)
	{
		return _mm_set_ps(0.0f, Ptr[2], Ptr[1], Ptr[0]);
	}

	inline void StoreFloat3(float* Ptr, FVectorRegister V)
	{
		alignas(16) float Temp[4];
		_mm_store_ps(Temp, V);

		Ptr[0] = Temp[0];
		Ptr[1] = Temp[1];
		Ptr[2] = Temp[2];
	}

	inline FVectorRegister LoadAligned(const float* Ptr) { return _mm_load_ps(Ptr); }
	inline void StoreAligned(float* Ptr, FVectorRegister V) { _mm_store_ps(Ptr, V); }

	inline FVectorRegister SetZero() { return _mm_setzero_ps(); }
	inline FVectorRegister SetWZero(FVectorRegister A)
	{
		return _mm_and_ps(A, _mm_castsi128_ps( _mm_set_epi32(0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)));
	}

	inline FVectorRegister SetVal(float F) { return _mm_set1_ps(F); }
	inline FVectorRegister SetVal(float X, float Y, float Z, float W) { return _mm_setr_ps(X, Y, Z, W); }
	
	inline FVectorRegister Add(FVectorRegister A, FVectorRegister B) { return _mm_add_ps(A, B); }
	inline FVectorRegister Sub(FVectorRegister A, FVectorRegister B) { return _mm_sub_ps(A, B); }
	inline FVectorRegister Mul(FVectorRegister A, FVectorRegister B) { return _mm_mul_ps(A, B); }
	inline FVectorRegister Div(FVectorRegister A, FVectorRegister B) { return _mm_div_ps(A, B); }

	inline FVectorRegister Rsqrt(FVectorRegister V) { return _mm_rsqrt_ps(V); }
	inline FVectorRegister Reciprocal(FVectorRegister A) { return _mm_div_ps(SetVal(1.0f), A); }

	inline FVectorRegister Min(FVectorRegister A, FVectorRegister B) { return _mm_min_ps(A, B); }
	inline FVectorRegister Max(FVectorRegister A, FVectorRegister B) { return _mm_max_ps(A, B); }
	
	inline FVectorRegister Abs(FVectorRegister A)
	{
		FVectorRegister Mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
		return _mm_and_ps(A, Mask);
	}

	inline bool Compare(FVectorRegister _a, FVectorRegister _b)
	{
		__m128 cmp = _mm_cmpeq_ps(_a, _b);
		return _mm_movemask_ps(cmp) == 0xF;
	}

	inline bool IsNearlyEqual(FVectorRegister A, FVectorRegister B, float Tolerance = 1e-4f)
	{
		// Step 1: 뺄셈 (A - B)
		FVectorRegister diff = _mm_sub_ps(A, B);

		// Step 2: abs
		// 0x7FFFFFFF (이진수로 01111111...) 마스크 생성
		// 이것과 AND 연산을 하면 맨 앞의 부호 비트가 무조건 0(양수)
		// _mm_castsi128_ps - 다시 float4 형태로 캐스팅
		FVectorRegister absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
		FVectorRegister absDiff = _mm_and_ps(diff, absMask);

		// Step 3: 엡실론 레지스터 준비 (4칸 모두 엡실론 값으로 꽉 채움)
		FVectorRegister epsReg = _mm_set1_ps(Tolerance);

		// Step 4: 작거나 같은가? (Compare Less-than or Equal)
		// absDiff <= epsReg 를 검사하여 맞으면 0xFFFFFFFF, 틀리면 0x0 을 반환
		FVectorRegister cmp = _mm_cmple_ps(absDiff, epsReg);

		// Step 5: 4개의 결과가 모두 0xFFFFFFFF(True) 인지 압축해서 확인
		return _mm_movemask_ps(cmp) == 0xF;
	}

	inline bool IsNearlyZero(FVectorRegister A, float Tolerance = 1e-4f)
	{
		// Step 1: abs
		// 0x7FFFFFFF (이진수로 01111111...) 마스크 생성
		// 이것과 AND 연산을 하면 맨 앞의 부호 비트가 무조건 0(양수)
		// _mm_castsi128_ps - 다시 float4 형태로 캐스팅
		FVectorRegister absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
		FVectorRegister absDiff = _mm_and_ps(A, absMask);

		// Step 3: 엡실론 레지스터 준비 (4칸 모두 엡실론 값으로 꽉 채움)
		FVectorRegister epsReg = _mm_set1_ps(Tolerance);

		// Step 4: 작거나 같은가? (Compare Less-than or Equal)
		// absDiff <= epsReg 를 검사하여 맞으면 0xFFFFFFFF, 틀리면 0x0 을 반환
		FVectorRegister cmp = _mm_cmple_ps(absDiff, epsReg);

		// Step 5: 4개의 결과가 모두 0xFFFFFFFF(True) 인지 압축해서 확인
		return _mm_movemask_ps(cmp) == 0xF;
	}

	// 내적 (SSE 4.1 이상)
	inline float Dot(FVectorRegister A, FVectorRegister B) { return _mm_cvtss_f32(_mm_dp_ps(A, B, 0xFF)); }
	inline FVectorRegister Dot4(FVectorRegister A, FVectorRegister B){ return _mm_dp_ps(A, B, 0xFF); }

	// 데이터 재배치 (외적 구현 시 필수)
	// 셔플은 매크로를 사용해야 하므로 템플릿으로 감싸는 것이 일반적
	template<int X, int Y, int Z, int W>
	inline FVectorRegister Shuffle(FVectorRegister A, FVectorRegister B)
	{
		return _mm_shuffle_ps(A, B, _MM_SHUFFLE(W, Z, Y, X));
		// 입력: Shuffle<0, 1, 2, 3>(A, B)  결과: [A0, A1, B2, B3]
	}

	template<int X, int Y, int Z, int W>
	inline FVectorRegister Swizzle(FVectorRegister A)
	{
		return _mm_shuffle_ps(A, A, _MM_SHUFFLE(W, Z, Y, X));
		// A = [10, 20, 30, 40], Swizzle<0, 0, 0, 0>(A),  [10, 10, 10, 10]
	}

	// X, Y, Z, W 중 하나를 4칸에 모두 복사 (행렬 곱셈 필수품)
	inline FVectorRegister SplatX(FVectorRegister A) { return Swizzle<0, 0, 0, 0>(A); }
	inline FVectorRegister SplatY(FVectorRegister A) { return Swizzle<1, 1, 1, 1>(A); }
	inline FVectorRegister SplatZ(FVectorRegister A) { return Swizzle<2, 2, 2, 2>(A); }
	inline FVectorRegister SplatW(FVectorRegister A) { return Swizzle<3, 3, 3, 3>(A); }

	inline FVectorRegister And(FVectorRegister A, FVectorRegister B) { return _mm_and_ps(A, B); }
	inline FVectorRegister Or(FVectorRegister A, FVectorRegister B) { return _mm_or_ps(A, B); }
	inline FVectorRegister Xor(FVectorRegister A, FVectorRegister B) { return _mm_xor_ps(A, B); }
	inline FVectorRegister AndNot(FVectorRegister A, FVectorRegister B) { return _mm_andnot_ps(A, B); } 

	inline FVectorRegister Cross3( FVectorRegister A, FVectorRegister B)
	{
		FVectorRegister A_yzx = Shuffle<1, 2, 0, 3>(A, A);
		FVectorRegister B_zxy = Shuffle<2, 0, 1, 3>(B, B);

		FVectorRegister A_zxy = Shuffle<2, 0, 1, 3>(A, A);
		FVectorRegister B_yzx = Shuffle<1, 2, 0, 3>(B, B);

		FVectorRegister Result = Sub(Mul(A_yzx, B_zxy), Mul(A_zxy, B_yzx));

		return _mm_and_ps(Result, _mm_castsi128_ps(_mm_set_epi32(0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)));
	}

	inline FVectorRegister Negate(FVectorRegister A)
	{
		FVectorRegister SignMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
		return _mm_xor_ps(A, SignMask);
	}

	inline FVectorRegister MultiplyAdd(FVectorRegister A, FVectorRegister B, FVectorRegister C)
	{
		return Add(Mul(A, B), C);
	}

	inline FVectorRegister NegativeMultiplyAdd( FVectorRegister A, FVectorRegister B, FVectorRegister C)
	{
		return Sub(C, Mul(A, B));
	}

	inline float HorizontalAdd4(FVectorRegister V)
	{
		FVectorRegister T = _mm_shuffle_ps(V, V, _MM_SHUFFLE(2, 3, 0, 1));

		V = _mm_add_ps(V, T);
		T = _mm_shuffle_ps(V, V, _MM_SHUFFLE(1, 0, 3, 2));
		V = _mm_add_ps(V, T);

		return _mm_cvtss_f32(V);
	}


	inline float LengthSquared(FVectorRegister A) { return Dot(A, A); }

	inline FVectorRegister Normalized(FVectorRegister A)
	{
		FVectorRegister LenSq = Dot4(A, A);
		FVectorRegister InvLen = Rsqrt(LenSq);
		// return Mul(A, InvLen);

		// InvLen = InvLen * (3 - LenSq * InvLen * InvLen) * 0.5
		FVectorRegister Half = SetVal(0.5f);
		FVectorRegister Three = SetVal(3.0f);

		InvLen = Mul(Mul(Half, InvLen), Sub(Three, Mul( LenSq, Mul(InvLen, InvLen))));

		return Mul(A, InvLen);

	}

	// Matrix 관련
	inline void Transpose(FVectorRegister& A, FVectorRegister& B, FVectorRegister& C, FVectorRegister& D)
	{
		_MM_TRANSPOSE4_PS(A, B, C, D);
	}


}


	