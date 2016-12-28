/*===---- fxsrintrin.h - FXSR intrinsic ------------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// fxsrintrin.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef __IMMINTRIN_H
#error "Never use <fxsrintrin.h> directly; include <immintrin.h> instead."
#endif

#ifndef __FXSRINTRIN_H
#define __FXSRINTRIN_H

#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

static __inline__ void __DEFAULT_FN_ATTRS
_fxsave(void *__p) {
  return __builtin_ia32_fxsave(__p);
}

static __inline__ void __DEFAULT_FN_ATTRS
_fxsave64(void *__p) {
  return __builtin_ia32_fxsave64(__p);
}

static __inline__ void __DEFAULT_FN_ATTRS
_fxrstor(void *__p) {
  return __builtin_ia32_fxrstor(__p);
}

static __inline__ void __DEFAULT_FN_ATTRS
_fxrstor64(void *__p) {
  return __builtin_ia32_fxrstor64(__p);
}

#undef __DEFAULT_FN_ATTRS

#endif

