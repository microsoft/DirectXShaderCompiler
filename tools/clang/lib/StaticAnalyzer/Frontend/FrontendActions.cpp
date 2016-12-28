//===--- FrontendActions.cpp ----------------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FrontendActions.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/StaticAnalyzer/Frontend/FrontendActions.h"
#include "clang/StaticAnalyzer/Frontend/AnalysisConsumer.h"
#include "clang/StaticAnalyzer/Frontend/ModelConsumer.h"
using namespace clang;
using namespace ento;

std::unique_ptr<ASTConsumer>
AnalysisAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return CreateAnalysisConsumer(CI);
}

ParseModelFileAction::ParseModelFileAction(llvm::StringMap<Stmt *> &Bodies)
    : Bodies(Bodies) {}

std::unique_ptr<ASTConsumer>
ParseModelFileAction::CreateASTConsumer(CompilerInstance &CI,
                                        StringRef InFile) {
  return llvm::make_unique<ModelConsumer>(Bodies);
}
