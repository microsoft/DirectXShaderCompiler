//===- CompilerTest.cpp - Compiler macro tests ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Compile-only: simulate pdb.h being included before Compiler.h.
#define pure = 0
#include "llvm/Support/Compiler.h"

LLVM_READONLY int readOnly(const int *);
