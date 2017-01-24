/*===---- bmiintrin.h - BMI intrinsics -------------------------------------===
#if !defined __X86INTRIN_H && !defined __IMMINTRIN_H
#error "Never use <bmiintrin.h> directly; include <x86intrin.h> instead."
#endif

#ifndef __BMI__
# error "BMI instruction set not enabled"
#endif /* __BMI__ */

#ifndef __BMIINTRIN_H
#define __BMIINTRIN_H

#define _tzcnt_u16(a)     (__tzcnt_u16((a)))
#define _andn_u32(a, b)   (__andn_u32((a), (b)))
/* _bextr_u32 != __bextr_u32 */
#define _blsi_u32(a)      (__blsi_u32((a)))
#define _blsmsk_u32(a)    (__blsmsk_u32((a)))
#define _blsr_u32(a)      (__blsr_u32((a)))
#define _tzcnt_u32(a)     (__tzcnt_u32((a)))

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ unsigned short __DEFAULT_FN_ATTRS
__tzcnt_u16(unsigned short __X)
{
  return __X ? __builtin_ctzs(__X) : 16;
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__andn_u32(unsigned int __X, unsigned int __Y)
{
  return ~__X & __Y;
}

/* AMD-specified, double-leading-underscore version of BEXTR */
static __inline__ unsigned int __DEFAULT_FN_ATTRS
__bextr_u32(unsigned int __X, unsigned int __Y)
{
  return __builtin_ia32_bextr_u32(__X, __Y);
}

/* Intel-specified, single-leading-underscore version of BEXTR */
static __inline__ unsigned int __DEFAULT_FN_ATTRS
_bextr_u32(unsigned int __X, unsigned int __Y, unsigned int __Z)
{
  return __builtin_ia32_bextr_u32 (__X, ((__Y & 0xff) | ((__Z & 0xff) << 8)));
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__blsi_u32(unsigned int __X)
{
  return __X & -__X;
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__blsmsk_u32(unsigned int __X)
{
  return __X ^ (__X - 1);
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__blsr_u32(unsigned int __X)
{
  return __X & (__X - 1);
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__tzcnt_u32(unsigned int __X)
{
  return __X ? __builtin_ctz(__X) : 32;
}

#ifdef __x86_64__

#define _andn_u64(a, b)   (__andn_u64((a), (b)))
/* _bextr_u64 != __bextr_u64 */
#define _blsi_u64(a)      (__blsi_u64((a)))
#define _blsmsk_u64(a)    (__blsmsk_u64((a)))
#define _blsr_u64(a)      (__blsr_u64((a)))
#define _tzcnt_u64(a)     (__tzcnt_u64((a)))

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__andn_u64 (unsigned long long __X, unsigned long long __Y)
{
  return ~__X & __Y;
}

/* AMD-specified, double-leading-underscore version of BEXTR */
static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__bextr_u64(unsigned long long __X, unsigned long long __Y)
{
  return __builtin_ia32_bextr_u64(__X, __Y);
}

/* Intel-specified, single-leading-underscore version of BEXTR */
static __inline__ unsigned long long __DEFAULT_FN_ATTRS
_bextr_u64(unsigned long long __X, unsigned int __Y, unsigned int __Z)
{
  return __builtin_ia32_bextr_u64 (__X, ((__Y & 0xff) | ((__Z & 0xff) << 8)));
}

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__blsi_u64(unsigned long long __X)
{
  return __X & -__X;
}

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__blsmsk_u64(unsigned long long __X)
{
  return __X ^ (__X - 1);
}

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__blsr_u64(unsigned long long __X)
{
  return __X & (__X - 1);
}

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__tzcnt_u64(unsigned long long __X)
{
  return __X ? __builtin_ctzll(__X) : 64;
}

#endif /* __x86_64__ */

#undef __DEFAULT_FN_ATTRS

#endif /* __BMIINTRIN_H */

