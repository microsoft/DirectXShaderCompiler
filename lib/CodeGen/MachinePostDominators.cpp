//===- MachinePostDominators.cpp -Machine Post Dominator Calculation ------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MachinePostDominators.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements simple dominator construction algorithms for finding //
// post dominators on machine functions.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/CodeGen/MachinePostDominators.h"

using namespace llvm;

char MachinePostDominatorTree::ID = 0;

//declare initializeMachinePostDominatorTreePass
INITIALIZE_PASS(MachinePostDominatorTree, "machinepostdomtree",
                "MachinePostDominator Tree Construction", true, true)

MachinePostDominatorTree::MachinePostDominatorTree() : MachineFunctionPass(ID) {
  initializeMachinePostDominatorTreePass(*PassRegistry::getPassRegistry());
  DT = new DominatorTreeBase<MachineBasicBlock>(true); //true indicate
                                                       // postdominator
}

FunctionPass *
MachinePostDominatorTree::createMachinePostDominatorTreePass() {
  return new MachinePostDominatorTree();
}

bool
MachinePostDominatorTree::runOnMachineFunction(MachineFunction &F) {
  DT->recalculate(F);
  return false;
}

MachinePostDominatorTree::~MachinePostDominatorTree() {
  delete DT;
}

void
MachinePostDominatorTree::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  MachineFunctionPass::getAnalysisUsage(AU);
}

void
MachinePostDominatorTree::print(llvm::raw_ostream &OS, const Module *M) const {
  DT->print(OS);
}
