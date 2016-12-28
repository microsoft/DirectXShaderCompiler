//===--- CommentDiagnostic.h - Diagnostics for the AST library --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CommentDiagnostic.h                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_AST_COMMENTDIAGNOSTIC_H
#define LLVM_CLANG_AST_COMMENTDIAGNOSTIC_H

#include "clang/Basic/Diagnostic.h"

namespace clang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define COMMENTSTART
#include "clang/Basic/DiagnosticCommentKinds.inc"
#undef DIAG
      NUM_BUILTIN_COMMENT_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace clang

#endif

