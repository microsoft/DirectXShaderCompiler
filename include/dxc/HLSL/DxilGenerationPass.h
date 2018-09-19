///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilGenerationPass.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
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
class StringRef;
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

// Pause/resume support.
bool ClearPauseResumePasses(llvm::Module &M); // true if modified; false if missing
void GetPauseResumePasses(llvm::Module &M, llvm::StringRef &pause, llvm::StringRef &resume);
void SetPauseResumePasses(llvm::Module &M, llvm::StringRef pause, llvm::StringRef resume);
}

namespace llvm {

/// \brief Create and return a pass that tranform the module into a DXIL module
/// Note that this pass is designed for use with the legacy pass manager.
ModulePass *createDxilCondenseResourcesPass();
ModulePass *createDxilLowerCreateHandleForLibPass();
ModulePass *createDxilAllocateResourcesForLibPass();
ModulePass *createDxilEliminateOutputDynamicIndexingPass();
ModulePass *createDxilGenerationPass(bool NotOptimized, hlsl::HLSLExtensionsCodegenHelper *extensionsHelper);
ModulePass *createHLEmitMetadataPass();
ModulePass *createHLEnsureMetadataPass();
ModulePass *createDxilFinalizeModulePass();
ModulePass *createDxilEmitMetadataPass();
FunctionPass *createDxilExpandTrigIntrinsicsPass();
ModulePass *createDxilConvergentMarkPass();
ModulePass *createDxilConvergentClearPass();
ModulePass *createDxilLoadMetadataPass();
ModulePass *createDxilDeadFunctionEliminationPass();
ModulePass *createHLDeadFunctionEliminationPass();
ModulePass *createHLPreprocessPass();
ModulePass *createDxilPrecisePropagatePass();
FunctionPass *createDxilPreserveAllOutputsPass();
FunctionPass *createDxilPromoteLocalResources();
ModulePass *createDxilPromoteStaticResources();
ModulePass *createDxilLegalizeResources();
ModulePass *createDxilLegalizeEvalOperationsPass();
FunctionPass *createDxilLegalizeSampleOffsetPass();
ModulePass *createFailUndefResourcePass();
FunctionPass *createSimplifyInstPass();
ModulePass *createDxilTranslateRawBuffer();
ModulePass *createNoPausePassesPass();
ModulePass *createPausePassesPass();
ModulePass *createResumePassesPass();
FunctionPass *createMatrixBitcastLowerPass();

void initializeDxilCondenseResourcesPass(llvm::PassRegistry&);
void initializeDxilLowerCreateHandleForLibPass(llvm::PassRegistry&);
void initializeDxilAllocateResourcesForLibPass(llvm::PassRegistry&);
void initializeDxilEliminateOutputDynamicIndexingPass(llvm::PassRegistry&);
void initializeDxilGenerationPassPass(llvm::PassRegistry&);
void initializeHLEnsureMetadataPass(llvm::PassRegistry&);
void initializeHLEmitMetadataPass(llvm::PassRegistry&);
void initializeDxilFinalizeModulePass(llvm::PassRegistry&);
void initializeDxilEmitMetadataPass(llvm::PassRegistry&);
void initializeDxilExpandTrigIntrinsicsPass(llvm::PassRegistry&);
void initializeDxilLoadMetadataPass(llvm::PassRegistry&);
void initializeDxilDeadFunctionEliminationPass(llvm::PassRegistry&);
void initializeHLDeadFunctionEliminationPass(llvm::PassRegistry&);
void initializeHLPreprocessPass(llvm::PassRegistry&);
void initializeDxilConvergentMarkPass(llvm::PassRegistry&);
void initializeDxilConvergentClearPass(llvm::PassRegistry&);
void initializeDxilPrecisePropagatePassPass(llvm::PassRegistry&);
void initializeDxilPreserveAllOutputsPass(llvm::PassRegistry&);
void initializeDxilPromoteLocalResourcesPass(llvm::PassRegistry&);
void initializeDxilPromoteStaticResourcesPass(llvm::PassRegistry&);
void initializeDxilLegalizeResourcesPass(llvm::PassRegistry&);
void initializeDxilLegalizeEvalOperationsPass(llvm::PassRegistry&);
void initializeDxilLegalizeSampleOffsetPassPass(llvm::PassRegistry&);
void initializeFailUndefResourcePass(llvm::PassRegistry&);
void initializeSimplifyInstPass(llvm::PassRegistry&);
void initializeDxilTranslateRawBufferPass(llvm::PassRegistry&);
void initializeNoPausePassesPass(llvm::PassRegistry&);
void initializePausePassesPass(llvm::PassRegistry&);
void initializeResumePassesPass(llvm::PassRegistry&);
void initializeMatrixBitcastLowerPassPass(llvm::PassRegistry&);

bool AreDxilResourcesDense(llvm::Module *M, hlsl::DxilResourceBase **ppNonDense);

}
