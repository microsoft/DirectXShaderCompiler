//===-- FrontendActions.h - Useful Frontend Actions -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FrontendActions.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
#ifndef LLVM_CLANG_REWRITE_FRONTEND_FRONTENDACTIONS_H
#define LLVM_CLANG_REWRITE_FRONTEND_FRONTENDACTIONS_H

#include "clang/Frontend/FrontendAction.h"

namespace clang {
class FixItRewriter;
class FixItOptions;

//===----------------------------------------------------------------------===//
// AST Consumer Actions
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

class HTMLPrintAction : public ASTFrontendAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override;
};

class FixItAction : public ASTFrontendAction {
protected:
  std::unique_ptr<FixItRewriter> Rewriter;
  std::unique_ptr<FixItOptions> FixItOpts;

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override;

  bool BeginSourceFileAction(CompilerInstance &CI,
                             StringRef Filename) override;

  void EndSourceFileAction() override;

  bool hasASTFileSupport() const override { return false; }

public:
  FixItAction();
  ~FixItAction() override;
};

/// \brief Emits changes to temporary files and uses them for the original
/// frontend action.
class FixItRecompile : public WrapperFrontendAction {
public:
  FixItRecompile(FrontendAction *WrappedAction)
    : WrapperFrontendAction(WrappedAction) {}

protected:
  bool BeginInvocation(CompilerInstance &CI) override;
};

class RewriteObjCAction : public ASTFrontendAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override;
};

class RewriteMacrosAction : public PreprocessorFrontendAction {
protected:
  void ExecuteAction() override;
};

class RewriteTestAction : public PreprocessorFrontendAction {
protected:
  void ExecuteAction() override;
};

class RewriteIncludesAction : public PreprocessorFrontendAction {
protected:
  void ExecuteAction() override;
};

}  // end namespace clang

#endif
