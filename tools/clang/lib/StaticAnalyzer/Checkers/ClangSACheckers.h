//===--- ClangSACheckers.h - Registration functions for Checkers *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ClangSACheckers.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Declares the registation functions for the checkers defined in            //
// libclangStaticAnalyzerCheckers.                                           //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_CLANGSACHECKERS_H
#define LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_CLANGSACHECKERS_H

#include "clang/StaticAnalyzer/Core/BugReporter/CommonBugCategories.h"

namespace clang {

namespace ento {
class CheckerManager;
class CheckerRegistry;

#define GET_CHECKERS
#define CHECKER(FULLNAME,CLASS,CXXFILE,HELPTEXT,GROUPINDEX,HIDDEN)    \
  void register##CLASS(CheckerManager &mgr);
#include "Checkers.inc"
#undef CHECKER
#undef GET_CHECKERS

} // end ento namespace

} // end clang namespace

#endif
