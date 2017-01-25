//===- llvm/Support/Win32/Host.inc ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Win32 Host support.
//
//===----------------------------------------------------------------------===//

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
