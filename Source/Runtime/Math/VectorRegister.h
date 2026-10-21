#pragma once

#if defined(__x86_64__) || defined(_M_X64)
// 인텔/AMD 환경
#include "VectorRegister_SSE.h"
//#elif defined(__aarch64__) || defined(_M_ARM64)
//// 애플 실리콘/모바일 환경 <-- 미구현
//#include <arm_neon.h>
//using VectorRegister = float32x4_t;
#else
struct alignas(16) VectorRegister { float v[4]; };
#endif

