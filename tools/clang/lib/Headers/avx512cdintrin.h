/*===------------- avx512cdintrin.h - AVX512CD intrinsics ------------------===

#ifndef __AVX512CDINTRIN_H
#define __AVX512CDINTRIN_H

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__, __target__("avx512cd")))

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_conflict_epi64 (__m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictdi_512_mask ((__v8di) __A,
                 (__v8di) _mm512_setzero_si512 (),
                 (__mmask8) -1);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_mask_conflict_epi64 (__m512i __W, __mmask8 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictdi_512_mask ((__v8di) __A,
               (__v8di) __W,
               (__mmask8) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_maskz_conflict_epi64 (__mmask8 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictdi_512_mask ((__v8di) __A,
                 (__v8di) _mm512_setzero_si512 (),
                 (__mmask8) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_conflict_epi32 (__m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictsi_512_mask ((__v16si) __A,
                 (__v16si) _mm512_setzero_si512 (),
                 (__mmask16) -1);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_mask_conflict_epi32 (__m512i __W, __mmask16 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictsi_512_mask ((__v16si) __A,
               (__v16si) __W,
               (__mmask16) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_maskz_conflict_epi32 (__mmask16 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vpconflictsi_512_mask ((__v16si) __A,
                 (__v16si) _mm512_setzero_si512 (),
                 (__mmask16) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_lzcnt_epi32 (__m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntd_512_mask ((__v16si) __A,
             (__v16si) _mm512_setzero_si512 (),
             (__mmask16) -1);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_mask_lzcnt_epi32 (__m512i __W, __mmask16 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntd_512_mask ((__v16si) __A,
                 (__v16si) __W,
                 (__mmask16) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_maskz_lzcnt_epi32 (__mmask16 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntd_512_mask ((__v16si) __A,
             (__v16si) _mm512_setzero_si512 (),
             (__mmask16) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_lzcnt_epi64 (__m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntq_512_mask ((__v8di) __A,
             (__v8di) _mm512_setzero_si512 (),
             (__mmask8) -1);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_mask_lzcnt_epi64 (__m512i __W, __mmask8 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntq_512_mask ((__v8di) __A,
                 (__v8di) __W,
                 (__mmask8) __U);
}

static __inline__ __m512i __DEFAULT_FN_ATTRS
_mm512_maskz_lzcnt_epi64 (__mmask8 __U, __m512i __A)
{
  return (__m512i) __builtin_ia32_vplzcntq_512_mask ((__v8di) __A,
             (__v8di) _mm512_setzero_si512 (),
             (__mmask8) __U);
}
#undef __DEFAULT_FN_ATTRS

#endif

