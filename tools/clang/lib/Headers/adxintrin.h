/*===---- adxintrin.h - ADX intrinsics -------------------------------------===
#ifndef __IMMINTRIN_H
#error "Never use <adxintrin.h> directly; include <immintrin.h> instead."
#endif

#ifndef __ADXINTRIN_H
#define __ADXINTRIN_H

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

/* Intrinsics that are available only if __ADX__ defined */
#ifdef __ADX__
static __inline unsigned char __DEFAULT_FN_ATTRS
_addcarryx_u32(unsigned char __cf, unsigned int __x, unsigned int __y,
               unsigned int *__p)
{
  return __builtin_ia32_addcarryx_u32(__cf, __x, __y, __p);
}

#ifdef __x86_64__
static __inline unsigned char __DEFAULT_FN_ATTRS
_addcarryx_u64(unsigned char __cf, unsigned long long __x,
               unsigned long long __y, unsigned long long  *__p)
{
  return __builtin_ia32_addcarryx_u64(__cf, __x, __y, __p);
}
#endif
#endif

/* Intrinsics that are also available if __ADX__ undefined */
static __inline unsigned char __DEFAULT_FN_ATTRS
_addcarry_u32(unsigned char __cf, unsigned int __x, unsigned int __y,
              unsigned int *__p)
{
  return __builtin_ia32_addcarry_u32(__cf, __x, __y, __p);
}

#ifdef __x86_64__
static __inline unsigned char __DEFAULT_FN_ATTRS
_addcarry_u64(unsigned char __cf, unsigned long long __x,
              unsigned long long __y, unsigned long long  *__p)
{
  return __builtin_ia32_addcarry_u64(__cf, __x, __y, __p);
}
#endif

static __inline unsigned char __DEFAULT_FN_ATTRS
_subborrow_u32(unsigned char __cf, unsigned int __x, unsigned int __y,
              unsigned int *__p)
{
  return __builtin_ia32_subborrow_u32(__cf, __x, __y, __p);
}

#ifdef __x86_64__
static __inline unsigned char __DEFAULT_FN_ATTRS
_subborrow_u64(unsigned char __cf, unsigned long long __x,
               unsigned long long __y, unsigned long long  *__p)
{
  return __builtin_ia32_subborrow_u64(__cf, __x, __y, __p);
}
#endif

#undef __DEFAULT_FN_ATTRS

#endif /* __ADXINTRIN_H */

