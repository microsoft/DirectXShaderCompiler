//===- CtorUtils.h - Helpers for working with global_ctors ------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CtorUtils.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines functions that are used to process llvm.global_ctors.   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TRANSFORMS_UTILS_CTORUTILS_H
#define LLVM_TRANSFORMS_UTILS_CTORUTILS_H

#include "llvm/ADT/STLExtras.h"

namespace llvm {

class GlobalVariable;
class Function;
class Module;

/// Call "ShouldRemove" for every entry in M's global_ctor list and remove the
/// entries for which it returns true.  Return true if anything changed.
bool optimizeGlobalCtorsList(Module &M,
                             function_ref<bool(Function *)> ShouldRemove);

} // End llvm namespace

#endif
