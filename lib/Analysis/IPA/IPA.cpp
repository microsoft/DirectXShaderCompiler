//===-- IPA.cpp -----------------------------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// IPA.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the common initialization routines for the IPA library.//
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/InitializePasses.h"
#include "llvm-c/Initialization.h"
#include "llvm/PassRegistry.h"

using namespace llvm;

/// initializeIPA - Initialize all passes linked into the IPA library.
void llvm::initializeIPA(PassRegistry &Registry) {
  initializeCallGraphWrapperPassPass(Registry);
  initializeCallGraphPrinterPass(Registry);
  initializeCallGraphViewerPass(Registry);
  initializeGlobalsModRefPass(Registry);
}

void LLVMInitializeIPA(LLVMPassRegistryRef R) {
  initializeIPA(*unwrap(R));
}
