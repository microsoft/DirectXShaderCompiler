/*===---- __wmmintrin_pclmul.h - AES intrinsics ----------------------------===

#if !defined (__PCLMUL__)
# error "PCLMUL instruction is not enabled"
#else
#define _mm_clmulepi64_si128(__X, __Y, __I) \
  ((__m128i)__builtin_ia32_pclmulqdq128((__v2di)(__m128i)(__X), \
                                        (__v2di)(__m128i)(__Y), (char)(__I)))
#endif

#endif /* _WMMINTRIN_PCLMUL_H */

