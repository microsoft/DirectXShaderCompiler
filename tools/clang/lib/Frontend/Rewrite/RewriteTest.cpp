//===--- RewriteTest.cpp - Rewriter playground ----------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RewriteTest.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This is a testbed.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/TokenRewriter.h"
#include "llvm/Support/raw_ostream.h"

void clang::DoRewriteTest(Preprocessor &PP, raw_ostream* OS) {
  SourceManager &SM = PP.getSourceManager();
  const LangOptions &LangOpts = PP.getLangOpts();

  TokenRewriter Rewriter(SM.getMainFileID(), SM, LangOpts);

  // Throw <i> </i> tags around comments.
  for (TokenRewriter::token_iterator I = Rewriter.token_begin(),
       E = Rewriter.token_end(); I != E; ++I) {
    if (I->isNot(tok::comment)) continue;

    Rewriter.AddTokenBefore(I, "<i>");
    Rewriter.AddTokenAfter(I, "</i>");
  }


  // Print out the output.
  for (TokenRewriter::token_iterator I = Rewriter.token_begin(),
       E = Rewriter.token_end(); I != E; ++I)
    *OS << PP.getSpelling(*I);
}
