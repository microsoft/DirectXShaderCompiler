//===--- DiagnosticAnalysis.h - Diagnostics for libanalysis -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AnalysisDiagnostic.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_ANALYSIS_ANALYSISDIAGNOSTIC_H
#define LLVM_CLANG_ANALYSIS_ANALYSISDIAGNOSTIC_H

#include "clang/Basic/Diagnostic.h"

namespace clang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define ANALYSISSTART
#include "clang/Basic/DiagnosticAnalysisKinds.inc"
#undef DIAG
      NUM_BUILTIN_ANALYSIS_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace clang

#endif
