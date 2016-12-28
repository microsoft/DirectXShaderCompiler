//== CheckerHelpers.h - Helper functions for checkers ------------*- C++ -*--=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CheckerHelpers.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines CheckerVisitor.                                        //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_CHECKERHELPERS_H
#define LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_CHECKERHELPERS_H

#include "clang/AST/Stmt.h"

namespace clang {

namespace ento {

bool containsMacro(const Stmt *S);
bool containsEnum(const Stmt *S);
bool containsStaticLocal(const Stmt *S);
bool containsBuiltinOffsetOf(const Stmt *S);
template <class T> bool containsStmt(const Stmt *S) {
  if (isa<T>(S))
      return true;

  for (const Stmt *Child : S->children())
    if (Child && containsStmt<T>(Child))
      return true;

  return false;
}

} // end GR namespace

} // end clang namespace

#endif
