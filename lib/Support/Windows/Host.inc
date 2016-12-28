//===- llvm/Support/Win32/Host.inc ------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Host.inc                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Win32 Host support.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "WindowsSupport.h"
#include <cstdio>
#include <string>

using namespace llvm;

// HLSL Change Starts - this is a fixed static char array now.
#if 0
std::string sys::getDefaultTargetTriple() {
  return Triple::normalize(LLVM_DEFAULT_TARGET_TRIPLE);
}
#else
const char *sys::getDefaultTargetTriple() {
  return "dxil-ms-dx";
}
#endif
// HLSL Change Ends - this is a fixed static char array now.
