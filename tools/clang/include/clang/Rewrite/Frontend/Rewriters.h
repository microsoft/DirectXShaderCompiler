//===--- Rewriters.h - Rewriter implementations     -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Rewriters.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This header contains miscellaneous utilities for various front-end actions.//
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_REWRITE_FRONTEND_REWRITERS_H
#define LLVM_CLANG_REWRITE_FRONTEND_REWRITERS_H

#include "clang/Basic/LLVM.h"

namespace clang {
class Preprocessor;
class PreprocessorOutputOptions;

/// RewriteMacrosInInput - Implement -rewrite-macros mode.
void RewriteMacrosInInput(Preprocessor &PP, raw_ostream *OS);

/// DoRewriteTest - A simple test for the TokenRewriter class.
void DoRewriteTest(Preprocessor &PP, raw_ostream *OS);

/// RewriteIncludesInInput - Implement -frewrite-includes mode.
void RewriteIncludesInInput(Preprocessor &PP, raw_ostream *OS,
                            const PreprocessorOutputOptions &Opts);

}  // end namespace clang

#endif
