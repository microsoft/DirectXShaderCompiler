/*===---- lzcntintrin.h - LZCNT intrinsics ---------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// lzcntintrin.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#if !defined __X86INTRIN_H && !defined __IMMINTRIN_H
#error "Never use <lzcntintrin.h> directly; include <x86intrin.h> instead."
#endif

#ifndef __LZCNT__
# error "LZCNT instruction is not enabled"
#endif /* __LZCNT__ */

#ifndef __LZCNTINTRIN_H
#define __LZCNTINTRIN_H

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ unsigned short __DEFAULT_FN_ATTRS
__lzcnt16(unsigned short __X)
{
  return __X ? __builtin_clzs(__X) : 16;
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
__lzcnt32(unsigned int __X)
{
  return __X ? __builtin_clz(__X) : 32;
}

static __inline__ unsigned int __DEFAULT_FN_ATTRS
_lzcnt_u32(unsigned int __X)
{
  return __X ? __builtin_clz(__X) : 32;
}

#ifdef __x86_64__
static __inline__ unsigned long long __DEFAULT_FN_ATTRS
__lzcnt64(unsigned long long __X)
{
  return __X ? __builtin_clzll(__X) : 64;
}

static __inline__ unsigned long long __DEFAULT_FN_ATTRS
_lzcnt_u64(unsigned long long __X)
{
  return __X ? __builtin_clzll(__X) : 64;
}
#endif

#undef __DEFAULT_FN_ATTRS

#endif /* __LZCNTINTRIN_H */

