//===- BarrierNoopPass.cpp - A barrier pass for the pass manager ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// BarrierNoopPass.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// NOTE: DO NOT USE THIS IF AVOIDABLE                                        //
//                                                                           //
// This pass is a nonce pass intended to allow manipulation of the implicitly//
// nesting pass manager. For example, it can be used to cause a CGSCC pass   //
// manager to be closed prior to running a new collection of function passes.//
//                                                                           //
// FIXME: This is a huge HACK. This should be removed when the pass manager's//
// nesting is made explicit instead of implicit.                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/Transforms/IPO.h"
using namespace llvm;

namespace {
/// \brief A nonce module pass used to place a barrier in a pass manager.
///
/// There is no mechanism for ending a CGSCC pass manager once one is started.
/// This prevents extension points from having clear deterministic ordering
/// when they are phrased as non-module passes.
class BarrierNoop : public ModulePass {
public:
  static char ID; // Pass identification.

  BarrierNoop() : ModulePass(ID) {
    initializeBarrierNoopPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override { return false; }
};
}

ModulePass *llvm::createBarrierNoopPass() { return new BarrierNoop(); }

char BarrierNoop::ID = 0;
INITIALIZE_PASS(BarrierNoop, "barrier", "A No-Op Barrier Pass",
                false, false)
