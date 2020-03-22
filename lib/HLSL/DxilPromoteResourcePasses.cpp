///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPromoteResourcePasses.cpp                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/HLModule.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace hlsl;

// Legalize resource use.
// Map local or static global resource to global resource.
// Require inline for static global resource.

namespace {

static const StringRef kStaticResourceLibErrorMsg = "static global resource use is disallowed in library exports.";

class DxilPromoteStaticResources : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPromoteStaticResources()
      : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize Static Resource Use";
  }

  bool runOnModule(Module &M) override {
    // Promote static global variables.
    return PromoteStaticGlobalResources(M);
  }

private:
  bool PromoteStaticGlobalResources(Module &M);
};

char DxilPromoteStaticResources::ID = 0;

class DxilPromoteLocalResources : public FunctionPass {
  void getAnalysisUsage(AnalysisUsage &AU) const override;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPromoteLocalResources()
      : FunctionPass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize Resource Use";
  }

  bool runOnFunction(Function &F) override {
    // Promote local resource first.
    return PromoteLocalResource(F);
  }

private:
  bool PromoteLocalResource(Function &F);
};

char DxilPromoteLocalResources::ID = 0;

}

void DxilPromoteLocalResources::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.setPreservesAll();
}

bool DxilPromoteLocalResources::PromoteLocalResource(Function &F) {
  bool bModified = false;
  std::vector<AllocaInst *> Allocas;
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache &AC =
      getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);

  BasicBlock &BB = F.getEntryBlock();
  unsigned allocaSize = 0;
  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) { // Is it an alloca?
        if (dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(AI->getAllocatedType()))) {
          if (isAllocaPromotable(AI))
            Allocas.push_back(AI);
        }
      }
    if (Allocas.empty())
      break;

    // No update.
    // Report error and break.
    if (allocaSize == Allocas.size()) {
      F.getContext().emitError(dxilutil::kResourceMapErrorMsg);
      break;
    }
    allocaSize = Allocas.size();

    PromoteMemToReg(Allocas, *DT, nullptr, &AC);
    bModified = true;
  }

  return bModified;
}

FunctionPass *llvm::createDxilPromoteLocalResources() {
  return new DxilPromoteLocalResources();
}

INITIALIZE_PASS_BEGIN(DxilPromoteLocalResources,
                      "hlsl-dxil-promote-local-resources",
                      "DXIL promote local resource use", false, true)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(DxilPromoteLocalResources,
                    "hlsl-dxil-promote-local-resources",
                    "DXIL promote local resource use", false, true)

bool DxilPromoteStaticResources::PromoteStaticGlobalResources(
    Module &M) {
  if (M.GetOrCreateHLModule().GetShaderModel()->IsLib()) {
    // Read/write to global static resource is disallowed for libraries:
    // Resource use needs to be resolved to a single real global resource,
    // but it may not be possible since any external function call may re-enter
    // at any other library export, which could modify the global static
    // between write and read.
    // While it could work for certain cases, describing the boundary at
    // the HLSL level is difficult, so at this point it's better to disallow.
    // example of what could work:
    //  After inlining, exported functions must have writes to static globals
    //  before reads, and must not have any external function calls between
    //  writes and subsequent reads, such that the static global may be
    //  optimized away for the exported function.
    for (auto &GV : M.globals()) {
      if (GV.getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage &&
        dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(GV.getType()))) {
        if (!GV.user_empty()) {
          if (Instruction *I = dyn_cast<Instruction>(*GV.user_begin())) {
            dxilutil::EmitErrorOnInstruction(I, kStaticResourceLibErrorMsg);
            break;
          }
        }
      }
    }
    return false;
  }

  bool bModified = false;
  std::set<GlobalVariable *> staticResources;
  for (auto &GV : M.globals()) {
    if (GV.getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage &&
        dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(GV.getType()))) {
      staticResources.insert(&GV);
    }
  }
  SSAUpdater SSA;
  SmallVector<Instruction *, 4> Insts;
  // Make sure every resource load has mapped to global variable.
  while (!staticResources.empty()) {
    bool bUpdated = false;
    for (auto it = staticResources.begin(); it != staticResources.end();) {
      GlobalVariable *GV = *(it++);
      // Build list of instructions to promote.
      for (User *U : GV->users()) {
        if (isa<LoadInst>(U) || isa<StoreInst>(U)) {
          Insts.emplace_back(cast<Instruction>(U));
        } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
          for (User *gepU : GEP->users()) {
            DXASSERT_NOMSG(isa<LoadInst>(gepU) || isa<StoreInst>(gepU));
            if (isa<LoadInst>(gepU) || isa<StoreInst>(gepU))
              Insts.emplace_back(cast<Instruction>(gepU));
          }
        } else {
          DXASSERT(false, "Unhandled user of resource static global");
        }
      }

      LoadAndStorePromoter(Insts, SSA).run(Insts);
      GV->removeDeadConstantUsers();
      if (GV->user_empty()) {
        bUpdated = true;
        staticResources.erase(GV);
      }

      Insts.clear();
    }
    if (!bUpdated) {
      M.getContext().emitError(dxilutil::kResourceMapErrorMsg);
      break;
    }
    bModified = true;
  }
  return bModified;
}

ModulePass *llvm::createDxilPromoteStaticResources() {
  return new DxilPromoteStaticResources();
}

INITIALIZE_PASS(DxilPromoteStaticResources,
                "hlsl-dxil-promote-static-resources",
                "DXIL promote static resource use", false, false)
