//===--- Refactoring.cpp - Framework for clang refactoring tools ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Refactoring.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  Implements tools to support refactorings.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_os_ostream.h"

namespace clang {
namespace tooling {

RefactoringTool::RefactoringTool(
    const CompilationDatabase &Compilations, ArrayRef<std::string> SourcePaths,
    std::shared_ptr<PCHContainerOperations> PCHContainerOps)
    : ClangTool(Compilations, SourcePaths, PCHContainerOps) {}

Replacements &RefactoringTool::getReplacements() { return Replace; }

int RefactoringTool::runAndSave(FrontendActionFactory *ActionFactory) {
  if (int Result = run(ActionFactory)) {
    return Result;
  }

  LangOptions DefaultLangOptions;
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);
  DiagnosticsEngine Diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
      &*DiagOpts, &DiagnosticPrinter, false);
  SourceManager Sources(Diagnostics, getFiles());
  Rewriter Rewrite(Sources, DefaultLangOptions);

  if (!applyAllReplacements(Rewrite)) {
    llvm::errs() << "Skipped some replacements.\n";
  }

  return saveRewrittenFiles(Rewrite);
}

bool RefactoringTool::applyAllReplacements(Rewriter &Rewrite) {
  return tooling::applyAllReplacements(Replace, Rewrite);
}

int RefactoringTool::saveRewrittenFiles(Rewriter &Rewrite) {
  return Rewrite.overwriteChangedFiles() ? 1 : 0;
}

} // end namespace tooling
} // end namespace clang
