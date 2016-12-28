//===--- DiagnosticLex.h - Diagnostics for liblex ---------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LexDiagnostic.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LEX_LEXDIAGNOSTIC_H
#define LLVM_CLANG_LEX_LEXDIAGNOSTIC_H

#include "clang/Basic/Diagnostic.h"

namespace clang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define LEXSTART
#include "clang/Basic/DiagnosticLexKinds.inc"
#undef DIAG
      NUM_BUILTIN_LEX_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace clang

#endif
