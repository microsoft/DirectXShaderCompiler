//===--- DiagnosticSema.h - Diagnostics for libsema -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaDiagnostic.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_SEMA_SEMADIAGNOSTIC_H
#define LLVM_CLANG_SEMA_SEMADIAGNOSTIC_H

#include "clang/Basic/Diagnostic.h"

namespace clang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define SEMASTART
#include "clang/Basic/DiagnosticSemaKinds.inc"
#undef DIAG
      NUM_BUILTIN_SEMA_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace clang

#endif
