//===- DiagnosticCategories.h - Diagnostic Categories Enumerators-*- C++ -*===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DiagnosticCategories.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_BASIC_DIAGNOSTICCATEGORIES_H
#define LLVM_CLANG_BASIC_DIAGNOSTICCATEGORIES_H

namespace clang {
  namespace diag {
    enum {
#define GET_CATEGORY_TABLE
#define CATEGORY(X, ENUM) ENUM,
#include "clang/Basic/DiagnosticGroups.inc"
#undef CATEGORY
#undef GET_CATEGORY_TABLE
      DiagCat_NUM_CATEGORIES
    };
  }  // end namespace diag
}  // end namespace clang

#endif
