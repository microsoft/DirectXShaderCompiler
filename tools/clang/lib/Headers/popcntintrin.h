/*===---- popcntintrin.h - POPCNT intrinsics -------------------------------===
#ifndef __POPCNT__
#error "POPCNT instruction set not enabled"
#endif

#ifndef _POPCNTINTRIN_H
#define _POPCNTINTRIN_H

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ int __DEFAULT_FN_ATTRS
_mm_popcnt_u32(unsigned int __A)
{
  return __builtin_popcount(__A);
}

#ifdef __x86_64__
static __inline__ long long __DEFAULT_FN_ATTRS
_mm_popcnt_u64(unsigned long long __A)
{
  return __builtin_popcountll(__A);
}
#endif /* __x86_64__ */

#undef __DEFAULT_FN_ATTRS

#endif /* _POPCNTINTRIN_H */

