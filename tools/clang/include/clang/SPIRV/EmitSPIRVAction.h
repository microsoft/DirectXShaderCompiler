//===-- EmitSPIRVAction.h - FrontendAction for Emitting SPIR-V --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_EMITSPIRVACTION_H
#define LLVM_CLANG_SPIRV_EMITSPIRVACTION_H

#include "clang/Frontend/FrontendAction.h"

#include "clang/SPIRV/EmitSPIRVOptions.h"

namespace clang {

class EmitSPIRVAction : public ASTFrontendAction {
public:
  EmitSPIRVAction(const EmitSPIRVOptions &opts,
                  llvm::ArrayRef<const char *> optsArray)
      : options(opts) {
    for (auto opt : optsArray)
      clOptions += " " + std::string(opt);
  }

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override;

private:
  EmitSPIRVOptions options;
  std::string clOptions;
};

} // end namespace clang

#endif