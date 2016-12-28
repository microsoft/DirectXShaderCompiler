//===--- Frontend/PCHContainerOperations.cpp - PCH Containers ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PCHContainerOperations.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines PCHContainerOperations and RawPCHContainerOperation.   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Frontend/PCHContainerOperations.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Lex/ModuleLoader.h"
using namespace clang;

namespace {

/// \brief A PCHContainerGenerator that writes out the PCH to a flat file.
class RawPCHContainerGenerator : public ASTConsumer {
  std::shared_ptr<PCHBuffer> Buffer;
  raw_pwrite_stream *OS;

public:
  RawPCHContainerGenerator(DiagnosticsEngine &Diags,
                           const HeaderSearchOptions &HSO,
                           const PreprocessorOptions &PPO,
                           const TargetOptions &TO, const LangOptions &LO,
                           const std::string &MainFileName,
                           const std::string &OutputFileName,
                           llvm::raw_pwrite_stream *OS,
                           std::shared_ptr<PCHBuffer> Buffer)
      : Buffer(Buffer), OS(OS) {}

  virtual ~RawPCHContainerGenerator() {}

  void HandleTranslationUnit(ASTContext &Ctx) override {
    if (Buffer->IsComplete) {
      // Make sure it hits disk now.
      *OS << Buffer->Data;
      OS->flush();
    }
    // Free the space of the temporary buffer.
    llvm::SmallVector<char, 0> Empty;
    Buffer->Data = std::move(Empty);
  }
};
}

std::unique_ptr<ASTConsumer> RawPCHContainerWriter::CreatePCHContainerGenerator(
    DiagnosticsEngine &Diags, const HeaderSearchOptions &HSO,
    const PreprocessorOptions &PPO, const TargetOptions &TO,
    const LangOptions &LO, const std::string &MainFileName,
    const std::string &OutputFileName, llvm::raw_pwrite_stream *OS,
    std::shared_ptr<PCHBuffer> Buffer) const {
  return llvm::make_unique<RawPCHContainerGenerator>(
      Diags, HSO, PPO, TO, LO, MainFileName, OutputFileName, OS, Buffer);
}

void RawPCHContainerReader::ExtractPCH(
    llvm::MemoryBufferRef Buffer, llvm::BitstreamReader &StreamFile) const {
  StreamFile.init((const unsigned char *)Buffer.getBufferStart(),
                  (const unsigned char *)Buffer.getBufferEnd());
}

PCHContainerOperations::PCHContainerOperations() {
  registerWriter(llvm::make_unique<RawPCHContainerWriter>());
  registerReader(llvm::make_unique<RawPCHContainerReader>());
}
