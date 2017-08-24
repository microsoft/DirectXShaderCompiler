//===--- EmitSPIRVAction.cpp - EmitSPIRVAction implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/EmitSPIRVAction.h"

#include "SPIRVEmitter.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/STLExtras.h"

namespace clang {

std::unique_ptr<ASTConsumer>
EmitSPIRVAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return llvm::make_unique<spirv::SPIRVEmitter>(CI, options);
}
} // end namespace clang
