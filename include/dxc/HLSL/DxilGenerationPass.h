///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilGenerationPass.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides a DXIL Generation pass.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class Module;
class ModulePass;
class Function;
class FunctionPass;
class Instruction;
class PassRegistry;
}

namespace hlsl {
class DxilResourceBase;
class WaveSensitivityAnalysis {
public:
  static WaveSensitivityAnalysis* create();
  virtual ~WaveSensitivityAnalysis() { }
  virtual void Analyze(llvm::Function *F) = 0;
  virtual bool IsWaveSensitive(llvm::Instruction *op) = 0;
};

class HLSLExtensionsCodegenHelper;
}

namespace llvm {

/// \brief Create and return a pass that tranform the module into a DXIL module
/// Note that this pass is designed for use with the legacy pass manager.
ModulePass *createDxilCondenseResourcesPass();
ModulePass *createDxilGenerationPass(bool NotOptimized, hlsl::HLSLExtensionsCodegenHelper *extensionsHelper);
ModulePass *createHLEmitMetadataPass();
ModulePass *createHLEnsureMetadataPass();
ModulePass *createDxilEmitMetadataPass();
ModulePass *createDxilPrecisePropagatePass();
FunctionPass *createSimplifyInstPass();

void initializeDxilCondenseResourcesPass(llvm::PassRegistry&);
void initializeDxilGenerationPassPass(llvm::PassRegistry&);
void initializeHLEnsureMetadataPass(llvm::PassRegistry&);
void initializeHLEmitMetadataPass(llvm::PassRegistry&);
void initializeDxilEmitMetadataPass(llvm::PassRegistry&);
void initializeDxilPrecisePropagatePassPass(llvm::PassRegistry&);
void initializeSimplifyInstPass(llvm::PassRegistry&);

bool AreDxilResourcesDense(llvm::Module *M, hlsl::DxilResourceBase **ppNonDense);

}
