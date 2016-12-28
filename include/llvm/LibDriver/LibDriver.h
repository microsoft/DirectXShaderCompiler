//===- llvm/LibDriver/LibDriver.h - lib.exe-compatible driver ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LibDriver.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Defines an interface to a lib.exe-compatible driver that also understands //
// bitcode files. Used by llvm-lib and lld-link2 /lib.                       //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIBDRIVER_LIBDRIVER_H
#define LLVM_LIBDRIVER_LIBDRIVER_H

#include "llvm/ADT/ArrayRef.h"

namespace llvm {

int libDriverMain(llvm::ArrayRef<const char*> ARgs);

}

#endif
