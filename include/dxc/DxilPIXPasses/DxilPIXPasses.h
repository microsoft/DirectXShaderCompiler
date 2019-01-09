///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPixPasses.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file provides a DXIL passes to support PIX.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class ModulePass;
class PassRegistry;

ModulePass *createDxilAddPixelHitInstrumentationPass();
ModulePass *createDxilAnnotateWithVirtualRegisterPass();
ModulePass *createDxilOutputColorBecomesConstantPass();
ModulePass *createDxilRemoveDiscardsPass();
ModulePass *createDxilReduceMSAAToSingleSamplePass();
ModulePass *createDxilForceEarlyZPass();
ModulePass *createDxilDebugInstrumentationPass();
ModulePass *createDxilShaderAccessTrackingPass();

void initializeDxilAddPixelHitInstrumentationPass(llvm::PassRegistry&);
void initializeDxilAnnotateWithVirtualRegisterPass(llvm::PassRegistry&);
void initializeDxilOutputColorBecomesConstantPass(llvm::PassRegistry&);
void initializeDxilRemoveDiscardsPass(llvm::PassRegistry&);
void initializeDxilReduceMSAAToSingleSamplePass(llvm::PassRegistry&);
void initializeDxilForceEarlyZPass(llvm::PassRegistry&);
void initializeDxilDebugInstrumentationPass(llvm::PassRegistry&);
void initializeDxilShaderAccessTrackingPass(llvm::PassRegistry&);

}
