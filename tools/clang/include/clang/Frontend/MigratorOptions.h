//===--- MigratorOptions.h - MigratorOptions Options ------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MigratorOptions.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This header contains the structures necessary for a front-end to specify  //
// various migration analysis.                                               //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_FRONTEND_MIGRATOROPTIONS_H
#define LLVM_CLANG_FRONTEND_MIGRATOROPTIONS_H

namespace clang {

class MigratorOptions {
public:
  unsigned NoNSAllocReallocError : 1;
  unsigned NoFinalizeRemoval : 1;
  MigratorOptions() {
    NoNSAllocReallocError = 0;
    NoFinalizeRemoval = 0;
  }
};

}
#endif
