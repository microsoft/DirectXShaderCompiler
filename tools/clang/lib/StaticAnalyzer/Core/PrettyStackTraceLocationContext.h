//==- PrettyStackTraceLocationContext.h - show analysis backtrace --*- C++ -*-//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PrettyStackTraceLocationContext.h                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LIB_STATICANALYZER_CORE_PRETTYSTACKTRACELOCATIONCONTEXT_H
#define LLVM_CLANG_LIB_STATICANALYZER_CORE_PRETTYSTACKTRACELOCATIONCONTEXT_H

#include "clang/Analysis/AnalysisContext.h"

namespace clang {
namespace ento {

/// While alive, includes the current analysis stack in a crash trace.
///
/// Example:
/// \code
/// 0.     Program arguments: ...
/// 1.     <eof> parser at end of file
/// 2.     While analyzing stack:
///        #0 void inlined()
///        #1 void test()
/// 3.     crash-trace.c:6:3: Error evaluating statement
/// \endcode
class PrettyStackTraceLocationContext : public llvm::PrettyStackTraceEntry {
  const LocationContext *LCtx;
public:
  PrettyStackTraceLocationContext(const LocationContext *LC) : LCtx(LC) {
    assert(LCtx);
  }

  void print(raw_ostream &OS) const override {
    OS << "While analyzing stack: \n";
    LCtx->dumpStack(OS, "\t");
  }
};

} // end ento namespace
} // end clang namespace

#endif
