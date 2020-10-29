///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPrecisePropagatePass.cpp                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperations.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace hlsl;

namespace {

typedef std::unordered_set<Value *> ValueSet;

class DxilPrecisePropagatePass : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPrecisePropagatePass() : ModulePass(ID) {}

  const char *getPassName() const override { return "DXIL Precise Propagate"; }

  bool runOnModule(Module &M) override {
    DxilModule &dxilModule = M.GetOrCreateDxilModule();
    DxilTypeSystem &typeSys = dxilModule.GetTypeSystem();
    ValueSet processedSet;
    std::vector<Function*> deadList;
    for (Function &F : M.functions()) {
      if (HLModule::HasPreciseAttribute(&F)) {
        PropagatePreciseOnFunctionUser(F, typeSys, processedSet);
        deadList.emplace_back(&F);
      }
    }
    for (Function *F : deadList)
      F->eraseFromParent();
    return true;
  }

private:
  void PropagatePreciseOnFunctionUser(
      Function &F, DxilTypeSystem &typeSys,
      ValueSet &processedSet);
};

char DxilPrecisePropagatePass::ID = 0;

}

static void PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys,
    ValueSet &processedSet);
static void PropagatePreciseAttributeOnPointer(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ValueSet &processedSet);
static void PropagatePreciseAttributeOnPointerUsers(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ValueSet &processedSet);
static void PropagatePreciseAttributeThroughGEPs(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ArrayRef<Value*> idxList, ValueSet &processedGEPs,
    ValueSet &processedSet);

static void PropagatePreciseAttributeOnOperand(
    Value *V, DxilTypeSystem &typeSys, LLVMContext &Context,
    ValueSet &processedSet) {
  // Skip values already marked.
  if (!processedSet.insert(V).second)
    return;

  if (V->getType()->isPointerTy()) {
    PropagatePreciseAttributeOnPointer(V, typeSys, Context, processedSet);
  }

  Instruction *I = dyn_cast<Instruction>(V);
  if (!I)
    return;

  // Set precise fast math on those instructions that support it.
  if (DxilModule::PreservesFastMathFlags(I))
    DxilModule::SetPreciseFastMathFlags(I);

  // Fast math not work on call, use metadata.
  if (isa<FPMathOperator>(I) && isa<CallInst>(I))
    HLModule::MarkPreciseAttributeWithMetadata(cast<CallInst>(I));
  PropagatePreciseAttribute(I, typeSys, processedSet);
}

static void PropagatePreciseAttributeThroughGEPs(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ArrayRef<Value*> idxList, ValueSet &processedGEPs,
    ValueSet &processedSet) {
  // recurse to matching GEP users
  for (User *U : Ptr->users()) {
    if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      // skip visited GEPs
      // These are separate from processedSet because while we don't need to
      // visit an intermediate GEP multiple times while marking a single value
      // precise, we are not necessarily marking every value reachable from
      // the GEP as precise, so we may need to revisit when marking a different
      // value as precise.
      if (!processedGEPs.insert(GEP).second)
        continue;

      // Mismatch if both constant and unequal, otherwise be conservative.
      bool bMismatch = false;
      auto idx = GEP->idx_begin();
      idx++;
      unsigned i = 0;
      while (idx != GEP->idx_end()) {
        if (ConstantInt *C = dyn_cast<ConstantInt>(*idx)) {
          if (ConstantInt *CRef = dyn_cast<ConstantInt>(idxList[i])) {
            if (CRef->getLimitedValue() != C->getLimitedValue()) {
              bMismatch = true;
              break;
            }
          }
        }
        idx++;
        i++;
      }
      if (bMismatch)
        continue;

      if ((unsigned)idxList.size() == i) {
        // Mark leaf users
        if (!processedSet.insert(GEP).second)
          continue;
        PropagatePreciseAttributeOnPointerUsers(GEP, typeSys, Context, processedSet);
      } else {
        // Recurse GEP users
        PropagatePreciseAttributeThroughGEPs(
            GEP, typeSys, Context,
            ArrayRef<Value*>(idxList.data() + i, idxList.end()),
            processedGEPs, processedSet);
      }
    }
  }
}

static void PropagatePreciseAttributeOnPointer(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ValueSet &processedSet) {

  PropagatePreciseAttributeOnPointerUsers(Ptr, typeSys, Context, processedSet);

  if (GEPOperator *GEP = dyn_cast<GEPOperator>(Ptr)) {
    // Get root Ptr, gather index list, and mark matching stores
    SmallVector<Value*, 8> idxList;
    SmallVector<GEPOperator*, 4> GEPs;
    GEPs.emplace_back(GEP);
    while (GEP = dyn_cast<GEPOperator>(Ptr = GEP->getPointerOperand()))
      GEPs.emplace_back(GEP);
    while (!GEPs.empty()) {
      GEP = GEPs.back();
      GEPs.pop_back();
      auto idx = GEP->idx_begin();
      idx++;
      while (idx != GEP->idx_end())
        idxList.emplace_back(*(idx++));
    }
    ValueSet processedGEPs;
    PropagatePreciseAttributeThroughGEPs(
        Ptr, typeSys, Context,
        idxList, processedGEPs, processedSet);
  }
}

static void PropagatePreciseAttributeOnPointerUsers(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    ValueSet &processedSet) {
  // Find all store and propagate on the val operand of store.
  // For CallInst, if Ptr is used as out parameter, mark it.
  for (User *U : Ptr->users()) {
    if (StoreInst *stInst = dyn_cast<StoreInst>(U)) {
      Value *val = stInst->getValueOperand();
      PropagatePreciseAttributeOnOperand(val, typeSys, Context, processedSet);
    } else if (CallInst *CI = dyn_cast<CallInst>(U)) {
      bool bReadOnly = true;

      Function *F = CI->getCalledFunction();

      // skip starting points (dx.attribute.precise calls)
      if (HLModule::HasPreciseAttribute(F))
        return;

      const DxilFunctionAnnotation *funcAnnotation =
          typeSys.GetFunctionAnnotation(F);

      if (funcAnnotation) {
        for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
          if (Ptr != CI->getArgOperand(i))
            continue;

          const DxilParameterAnnotation &paramAnnotation =
              funcAnnotation->GetParameterAnnotation(i);
          // OutputPatch and OutputStream will be checked after scalar repl.
          // Here only check out/inout
          if (paramAnnotation.GetParamInputQual() == DxilParamInputQual::Out ||
              paramAnnotation.GetParamInputQual() == DxilParamInputQual::Inout) {
            bReadOnly = false;
            break;
          }
        }
      } else {
        bReadOnly = false;
      }

      if (!bReadOnly)
        PropagatePreciseAttributeOnOperand(CI, typeSys, Context, processedSet);
    }
  }
}

static void
PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys,
                          ValueSet &processedSet) {
  LLVMContext &Context = I->getContext();
  for (Value *src : I->operands())
    PropagatePreciseAttributeOnOperand(src, typeSys, Context, processedSet);

  if (PHINode *Phi = dyn_cast<PHINode>(I)) {
    // TODO: For phi, we have to mark branch conditions that impact incoming paths.
  }
}

void DxilPrecisePropagatePass::PropagatePreciseOnFunctionUser(
    Function &F, DxilTypeSystem &typeSys,
    ValueSet &processedSet) {
  LLVMContext &Context = F.getContext();
  for (auto U = F.user_begin(), E = F.user_end(); U != E;) {
    CallInst *CI = cast<CallInst>(*(U++));
    Value *V = CI->getArgOperand(0);
    PropagatePreciseAttributeOnOperand(V, typeSys, Context, processedSet);
    CI->eraseFromParent();
  }
}

ModulePass *llvm::createDxilPrecisePropagatePass() {
  return new DxilPrecisePropagatePass();
}

INITIALIZE_PASS(DxilPrecisePropagatePass, "hlsl-dxil-precise", "DXIL precise attribute propagate", false, false)
