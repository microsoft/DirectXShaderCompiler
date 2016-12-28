//===--- Util.h - Common Driver Utilities -----------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Util.h                                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_DRIVER_UTIL_H
#define LLVM_CLANG_DRIVER_UTIL_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/DenseMap.h"

namespace clang {
class DiagnosticsEngine;

namespace driver {
  class Action;
  class JobAction;

  /// ArgStringMap - Type used to map a JobAction to its result file.
  typedef llvm::DenseMap<const JobAction*, const char*> ArgStringMap;

  /// ActionList - Type used for lists of actions.
  typedef SmallVector<Action*, 3> ActionList;

} // end namespace driver
} // end namespace clang

#endif
