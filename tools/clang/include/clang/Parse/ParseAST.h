//===--- ParseAST.h - Define the ParseAST method ----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ParseAST.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the clang::ParseAST method.                            //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_PARSE_PARSEAST_H
#define LLVM_CLANG_PARSE_PARSEAST_H

#include "clang/Basic/LangOptions.h"

namespace clang {
  class Preprocessor;
  class ASTConsumer;
  class ASTContext;
  class CodeCompleteConsumer;
  class Sema;

  /// \brief Parse the entire file specified, notifying the ASTConsumer as
  /// the file is parsed.
  ///
  /// This operation inserts the parsed decls into the translation
  /// unit held by Ctx.
  ///
  /// \param TUKind The kind of translation unit being parsed.
  ///
  /// \param CompletionConsumer If given, an object to consume code completion
  /// results.
  void ParseAST(Preprocessor &pp, ASTConsumer *C,
                ASTContext &Ctx, bool PrintStats = false,
                TranslationUnitKind TUKind = TU_Complete,
                CodeCompleteConsumer *CompletionConsumer = nullptr,
                bool SkipFunctionBodies = false);

  /// \brief Parse the main file known to the preprocessor, producing an 
  /// abstract syntax tree.
  void ParseAST(Sema &S, bool PrintStats = false,
                bool SkipFunctionBodies = false);
  
}  // end namespace clang

#endif
