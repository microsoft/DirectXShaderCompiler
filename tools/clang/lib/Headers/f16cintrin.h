/*===---- f16cintrin.h - F16C intrinsics -----------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// f16cintrin.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#if !defined __X86INTRIN_H && !defined __IMMINTRIN_H
#error "Never use <f16cintrin.h> directly; include <x86intrin.h> instead."
#endif

#ifndef __F16C__
# error "F16C instruction is not enabled"
#endif /* __F16C__ */

#ifndef __F16CINTRIN_H
#define __F16CINTRIN_H

typedef float __v8sf __attribute__ ((__vector_size__ (32)));
typedef float __m256 __attribute__ ((__vector_size__ (32)));

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

#define _mm_cvtps_ph(a, imm) __extension__ ({ \
  __m128 __a = (a); \
 (__m128i)__builtin_ia32_vcvtps2ph((__v4sf)__a, (imm)); })

#define _mm256_cvtps_ph(a, imm) __extension__ ({ \
  __m256 __a = (a); \
 (__m128i)__builtin_ia32_vcvtps2ph256((__v8sf)__a, (imm)); })

static __inline __m128 __DEFAULT_FN_ATTRS
_mm_cvtph_ps(__m128i __a)
{
  return (__m128)__builtin_ia32_vcvtph2ps((__v8hi)__a);
}

static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_cvtph_ps(__m128i __a)
{
  return (__m256)__builtin_ia32_vcvtph2ps256((__v8hi)__a);
}

#undef __DEFAULT_FN_ATTRS

#endif /* __F16CINTRIN_H */

