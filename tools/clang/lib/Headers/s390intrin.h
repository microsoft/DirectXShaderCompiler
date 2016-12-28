/*===---- s390intrin.h - SystemZ intrinsics --------------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// s390intrin.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef __S390INTRIN_H
#define __S390INTRIN_H

#ifndef __s390__
#error "<s390intrin.h> is for s390 only"
#endif

#ifdef __HTM__
#include <htmintrin.h>
#endif

#ifdef __VEC__
#include <vecintrin.h>
#endif

#endif /* __S390INTRIN_H*/

