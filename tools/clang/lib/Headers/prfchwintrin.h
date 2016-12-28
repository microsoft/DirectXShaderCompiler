/*===---- prfchwintrin.h - PREFETCHW intrinsic -----------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// prfchwintrin.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#if !defined(__X86INTRIN_H) && !defined(_MM3DNOW_H_INCLUDED)
#error "Never use <prfchwintrin.h> directly; include <x86intrin.h> or <mm3dnow.h> instead."
#endif

#ifndef __PRFCHWINTRIN_H
#define __PRFCHWINTRIN_H

#if defined(__PRFCHW__) || defined(__3dNOW__)
static __inline__ void __attribute__((__always_inline__, __nodebug__))
_m_prefetchw(void *__P)
{
  __builtin_prefetch (__P, 1, 3 /* _MM_HINT_T0 */);
}
#endif

#endif /* __PRFCHWINTRIN_H */

