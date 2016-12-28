/*====-------- ia32intrin.h ---------------------------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ia32intrin.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef __X86INTRIN_H
#error "Never use <ia32intrin.h> directly; include <x86intrin.h> instead."
#endif

#ifndef __IA32INTRIN_H
#define __IA32INTRIN_H

#ifdef __x86_64__
static __inline__ unsigned long long __attribute__((__always_inline__, __nodebug__))
__readeflags(void)
{
  unsigned long long __res = 0;
  __asm__ __volatile__ ("pushf\n\t"
                        "popq %0\n"
                        :"=r"(__res)
                        :
                        :
                       );
  return __res;
}

static __inline__ void __attribute__((__always_inline__, __nodebug__))
__writeeflags(unsigned long long __f)
{
  __asm__ __volatile__ ("pushq %0\n\t"
                        "popf\n"
                        :
                        :"r"(__f)
                        :"flags"
                       );
}

#else /* !__x86_64__ */
static __inline__ unsigned int __attribute__((__always_inline__, __nodebug__))
__readeflags(void)
{
  unsigned int __res = 0;
  __asm__ __volatile__ ("pushf\n\t"
                        "popl %0\n"
                        :"=r"(__res)
                        :
                        :
                       );
  return __res;
}

static __inline__ void __attribute__((__always_inline__, __nodebug__))
__writeeflags(unsigned int __f)
{
  __asm__ __volatile__ ("pushl %0\n\t"
                        "popf\n"
                        :
                        :"r"(__f)
                        :"flags"
                       );
}
#endif /* !__x86_64__ */

static __inline__ unsigned long long __attribute__((__always_inline__, __nodebug__))
__rdpmc(int __A) {
  return __builtin_ia32_rdpmc(__A);
}

/* __rdtsc */
static __inline__ unsigned long long __attribute__((__always_inline__, __nodebug__))
__rdtsc(void) {
  return __builtin_ia32_rdtsc();
}

/* __rdtscp */
static __inline__ unsigned long long __attribute__((__always_inline__, __nodebug__))
__rdtscp(unsigned int *__A) {
  return __builtin_ia32_rdtscp(__A);
}

#define _rdtsc() __rdtsc()

#endif /* __IA32INTRIN_H */

