//===-- MathExtras.cpp - Implement the MathExtras header --------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MathExtras.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the MathExtras.h header                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/MathExtras.h"

#ifdef _MSC_VER
#include <limits>
#else
#include <math.h>
#endif

namespace llvm {

#if defined(_MSC_VER)
  // Visual Studio defines the HUGE_VAL class of macros using purposeful
  // constant arithmetic overflow, which it then warns on when encountered.
  const float huge_valf = std::numeric_limits<float>::infinity();
#else
  const float huge_valf = HUGE_VALF;
#endif

}
