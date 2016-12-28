//===-- CodeInjector.h ------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CodeInjector.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines the clang::CodeInjector interface which is responsible for//
/// injecting AST of function definitions that may not be available in the   //
/// original source.                                                         //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_ANALYSIS_CODEINJECTOR_H
#define LLVM_CLANG_ANALYSIS_CODEINJECTOR_H

namespace clang {

class Stmt;
class FunctionDecl;
class ObjCMethodDecl;

/// \brief CodeInjector is an interface which is responsible for injecting AST
/// of function definitions that may not be available in the original source.
///
/// The getBody function will be called each time the static analyzer examines a
/// function call that has no definition available in the current translation
/// unit. If the returned statement is not a null pointer, it is assumed to be
/// the body of a function which will be used for the analysis. The source of
/// the body can be arbitrary, but it is advised to use memoization to avoid
/// unnecessary reparsing of the external source that provides the body of the
/// functions.
class CodeInjector {
public:
  CodeInjector();
  virtual ~CodeInjector();

  virtual Stmt *getBody(const FunctionDecl *D) = 0;
  virtual Stmt *getBody(const ObjCMethodDecl *D) = 0;
};
}

#endif
