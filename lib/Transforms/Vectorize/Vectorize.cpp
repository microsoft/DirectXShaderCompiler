//===-- Vectorize.cpp -----------------------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Vectorize.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements common infrastructure for libLLVMVectorizeOpts.a, which//
// implements several vectorization transformations over the LLVM intermediate//
// representation, including the C bindings for that library.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#if HLSL_VECTORIZATION_ENABLED // HLSL Change - don't build vectorization passes

#include "llvm/Transforms/Vectorize.h"
#include "llvm-c/Initialization.h"
#include "llvm-c/Transforms/Vectorize.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace llvm;

/// initializeVectorizationPasses - Initialize all passes linked into the
/// Vectorization library.
void llvm::initializeVectorization(PassRegistry &Registry) {
  initializeBBVectorizePass(Registry);
  initializeLoopVectorizePass(Registry);
  initializeSLPVectorizerPass(Registry);
}

void LLVMInitializeVectorization(LLVMPassRegistryRef R) {
  initializeVectorization(*unwrap(R));
}

void LLVMAddBBVectorizePass(LLVMPassManagerRef PM) {
  unwrap(PM)->add(createBBVectorizePass());
}

void LLVMAddLoopVectorizePass(LLVMPassManagerRef PM) {
  unwrap(PM)->add(createLoopVectorizePass());
}

void LLVMAddSLPVectorizePass(LLVMPassManagerRef PM) {
  unwrap(PM)->add(createSLPVectorizerPass());
}

#else

namespace llvm {
class PassRegistry;

void initializeVectorization(PassRegistry &) {}

}

#endif // HLSL Change - don't build vectorization passes
