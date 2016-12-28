/*===---- pmmintrin.h - SSE3 intrinsics ------------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// pmmintrin.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
/////////////////////////////////////////////////////////////////////////////// 
#ifndef __PMMINTRIN_H
#define __PMMINTRIN_H

#ifndef __SSE3__
#error "SSE3 instruction set not enabled"
#else

#include <emmintrin.h>

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_lddqu_si128(__m128i const *__p)
{
  return (__m128i)__builtin_ia32_lddqu((char const *)__p);
}

static __inline__ __m128 __DEFAULT_FN_ATTRS
_mm_addsub_ps(__m128 __a, __m128 __b)
{
  return __builtin_ia32_addsubps(__a, __b);
}

static __inline__ __m128 __DEFAULT_FN_ATTRS
_mm_hadd_ps(__m128 __a, __m128 __b)
{
  return __builtin_ia32_haddps(__a, __b);
}

static __inline__ __m128 __DEFAULT_FN_ATTRS
_mm_hsub_ps(__m128 __a, __m128 __b)
{
  return __builtin_ia32_hsubps(__a, __b);
}

static __inline__ __m128 __DEFAULT_FN_ATTRS
_mm_movehdup_ps(__m128 __a)
{
  return __builtin_shufflevector(__a, __a, 1, 1, 3, 3);
}

static __inline__ __m128 __DEFAULT_FN_ATTRS
_mm_moveldup_ps(__m128 __a)
{
  return __builtin_shufflevector(__a, __a, 0, 0, 2, 2);
}

static __inline__ __m128d __DEFAULT_FN_ATTRS
_mm_addsub_pd(__m128d __a, __m128d __b)
{
  return __builtin_ia32_addsubpd(__a, __b);
}

static __inline__ __m128d __DEFAULT_FN_ATTRS
_mm_hadd_pd(__m128d __a, __m128d __b)
{
  return __builtin_ia32_haddpd(__a, __b);
}

static __inline__ __m128d __DEFAULT_FN_ATTRS
_mm_hsub_pd(__m128d __a, __m128d __b)
{
  return __builtin_ia32_hsubpd(__a, __b);
}

#define        _mm_loaddup_pd(dp)        _mm_load1_pd(dp)

static __inline__ __m128d __DEFAULT_FN_ATTRS
_mm_movedup_pd(__m128d __a)
{
  return __builtin_shufflevector(__a, __a, 0, 0);
}

#define _MM_DENORMALS_ZERO_ON   (0x0040)
#define _MM_DENORMALS_ZERO_OFF  (0x0000)

#define _MM_DENORMALS_ZERO_MASK (0x0040)

#define _MM_GET_DENORMALS_ZERO_MODE() (_mm_getcsr() & _MM_DENORMALS_ZERO_MASK)
#define _MM_SET_DENORMALS_ZERO_MODE(x) (_mm_setcsr((_mm_getcsr() & ~_MM_DENORMALS_ZERO_MASK) | (x)))

static __inline__ void __DEFAULT_FN_ATTRS
_mm_monitor(void const *__p, unsigned __extensions, unsigned __hints)
{
  __builtin_ia32_monitor((void *)__p, __extensions, __hints);
}

static __inline__ void __DEFAULT_FN_ATTRS
_mm_mwait(unsigned __extensions, unsigned __hints)
{
  __builtin_ia32_mwait(__extensions, __hints);
}

#undef __DEFAULT_FN_ATTRS

#endif /* __SSE3__ */

#endif /* __PMMINTRIN_H */

