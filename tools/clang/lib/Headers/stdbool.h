/*===---- stdbool.h - Standard header for booleans -------------------------===
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// stdbool.h                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef __STDBOOL_H
#define __STDBOOL_H

/* Don't define bool, true, and false in C++, except as a GNU extension. */
#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#elif defined(__GNUC__) && !defined(__STRICT_ANSI__)
/* Define _Bool, bool, false, true as a GNU extension. */
#define _Bool bool
#define bool  bool
#define false false
#define true  true
#endif

#define __bool_true_false_are_defined 1

#endif /* __STDBOOL_H */

