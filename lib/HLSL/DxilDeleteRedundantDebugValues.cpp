///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilDeleteRedundantDebugValues.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//
// Removes as many dbg.value's as possible:
//
// 1. Search for all scopes (and their parent scopes) that have any real (non-debug)
//    instructions at all.
//
// 2. For each dbg.value, if it's refering to a variable from a scope not in
//    the set of scopes from step 1, then delete it.
//
// 3. In any contiguous series of dbg.value instructions, if there are dbg.value's
//    that point to the same variable+fragment, then delete all but the last one,
//    since it would be the only authentic mapping for that variable fragment.

#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <unordered_set>

using namespace llvm;

namespace {
class DxilDeleteRedundantDebugValues : public ModulePass {
public:
  static char ID;

  explicit DxilDeleteRedundantDebugValues() : ModulePass(ID) {
    initializeDxilDeleteRedundantDebugValuesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;
};
char DxilDeleteRedundantDebugValues::ID;
}

bool DxilDeleteRedundantDebugValues::runOnModule(Module &M) {
  if (!llvm::hasDebugInfo(M))
    return false;

  bool Changed = false;

  unsigned NumDbgInsts = 0;
  unsigned NumNonDbgInsts = 0;
  for (Function &F : M) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (isa<DbgInfoIntrinsic>(I))
          NumDbgInsts++;
        else
          NumNonDbgInsts++;
      }
    }
  }

  // Here is an arbitrary threshold where we decide there's too many
  // debug instructions.
  // TODO: Issue a warning here about removing debug instructions.
  bool TryReduceDbgInsts = NumDbgInsts >= 10*NumNonDbgInsts && NumDbgInsts >= 100000;

  std::unordered_set<DILocalScope *> SeenScopes;
  typedef std::pair<DILocalVariable *, DIExpression *> VarPair;
  SmallDenseMap<VarPair, DbgValueInst *> SeenVar;
  SmallVector<DILocalScope *, 8> ScopeWorklist;

  for (Function &F : M) {
    SeenScopes.clear();
    SeenVar.clear();

    // Collect a set of all scopes that non-debug instructions
    // are attached to. There's no need to keep debug info for
    // any variables for any scopes that
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (isa<DbgInfoIntrinsic>(I))
          continue;
        DebugLoc DL = I.getDebugLoc();
        if (!DL) continue;

        ScopeWorklist.clear();
        if (!TryReduceDbgInsts) {
          if (DILocation *InlinedAt = DL.getInlinedAt()) {
            if (DILocalScope *Scope = cast_or_null<DILocalScope>(InlinedAt->getScope()))
              ScopeWorklist.push_back(Scope);
          }
        }
        if (DILocalScope *Scope = cast_or_null<DILocalScope>(DL.getScope())) {
          ScopeWorklist.push_back(Scope);
        }

        if (ScopeWorklist.empty())
          continue;

        while (ScopeWorklist.size()) {
          DILocalScope *Scope = ScopeWorklist.pop_back_val();
          SeenScopes.insert(Scope);
          if (DILexicalBlockBase *LB = dyn_cast<DILexicalBlockBase>(Scope)) {
            ScopeWorklist.push_back(LB->getScope());
          }
        }
      }
    }

    for (BasicBlock &BB : F) {
      for (auto it = BB.begin(), end = BB.end(); it != end;) {
        Instruction &I = *(it++);
        if (!isa<DbgInfoIntrinsic>(I)) {
          SeenVar.clear();
          continue;
        }

        DbgValueInst *DI = dyn_cast<DbgValueInst>(&I);
        if (!DI) continue;
        DILocalVariable *Var  = DI->getVariable();
        DIExpression    *Expr = DI->getExpression();
        VarPair Pair = VarPair(Var, Expr);

        if (!SeenScopes.count(Var->getScope())) {
          Changed = true;
          DI->eraseFromParent();
          continue;
        }

        auto findIt = SeenVar.find(Pair);
        if (findIt != SeenVar.end()) {
          findIt->second->eraseFromParent();
          findIt->second = DI;
          Changed = true;
        }
        else {
          SeenVar[Pair] = DI;
        }
      }
    }
  }

  return Changed;
}

ModulePass *llvm::createDxilDeleteRedundantDebugValuesPass() {
  return new DxilDeleteRedundantDebugValues();
}

INITIALIZE_PASS(DxilDeleteRedundantDebugValues, "dxil-delete-redundant-debug-values", "Dxil Delete Redundant Debug Values", false, false)
