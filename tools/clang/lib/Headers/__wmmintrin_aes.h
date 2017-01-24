/*===---- __wmmintrin_aes.h - AES intrinsics -------------------------------===

#include <emmintrin.h>

#if !defined (__AES__)
#  error "AES instructions not enabled"
#else

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_aesenc_si128(__m128i __V, __m128i __R)
{
  return (__m128i)__builtin_ia32_aesenc128(__V, __R);
}

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_aesenclast_si128(__m128i __V, __m128i __R)
{
  return (__m128i)__builtin_ia32_aesenclast128(__V, __R);
}

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_aesdec_si128(__m128i __V, __m128i __R)
{
  return (__m128i)__builtin_ia32_aesdec128(__V, __R);
}

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_aesdeclast_si128(__m128i __V, __m128i __R)
{
  return (__m128i)__builtin_ia32_aesdeclast128(__V, __R);
}

static __inline__ __m128i __DEFAULT_FN_ATTRS
_mm_aesimc_si128(__m128i __V)
{
  return (__m128i)__builtin_ia32_aesimc128(__V);
}

#define _mm_aeskeygenassist_si128(C, R) \
  __builtin_ia32_aeskeygenassist128((C), (R))

#undef __DEFAULT_FN_ATTRS

#endif

#endif  /* _WMMINTRIN_AES_H */

