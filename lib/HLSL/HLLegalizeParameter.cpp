///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLLegalizeParameter.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Legalize in parameter has write and out parameter has read.               //
// Must be call before inline pass.                                          //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/DxilGenerationPass.h"

#include "dxc/DXIL/DxilTypeSystem.h"

#include "llvm/IR/IntrinsicInst.h"

#include "dxc/Support/Global.h"
#include "llvm/Pass.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"

#include <vector>

using namespace llvm;
using namespace hlsl;

// For parameter need to legalize, create alloca to replace all uses of it, and copy between the alloca and the parameter.

namespace {

struct PointerStatus {
  /// Keep if the pointer has be stored/loaded.
  bool  hasStore;
  bool  hasLoad;
  /// Look at all uses of the global and fill in the GlobalStatus structure.  If
  /// the global has its address taken, return true to indicate we can't do
  /// anything with it.
  static void analyzePointer(const Value *V, PointerStatus &PS,
                             DxilTypeSystem &typeSys);

  PointerStatus()
      : hasStore(false), hasLoad(false) {}
};

void PointerStatus::analyzePointer(const Value *V, PointerStatus &PS,
                                   DxilTypeSystem &typeSys) {
  if (PS.hasStore && PS.hasLoad)
    return;

  for (const User *U : V->users()) {

    if (const BitCastOperator *BC = dyn_cast<BitCastOperator>(U)) {
      analyzePointer(BC, PS, typeSys);
    } else if (const MemCpyInst *MC = dyn_cast<MemCpyInst>(U)) {
      if (MC->getRawDest() == V) {
        PS.hasStore = true;
      } else {
        DXASSERT(MC->getRawSource() == V, "must be source here");
        PS.hasLoad = true;
      }
    } else if (const GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      analyzePointer(GEP, PS, typeSys);
    } else if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
        PS.hasStore = true;
    } else if (dyn_cast<LoadInst>(U)) {
      PS.hasLoad = true;
    } else if (const CallInst *CI = dyn_cast<CallInst>(U)) {
      Function *F = CI->getCalledFunction();
      DxilFunctionAnnotation *annotation = typeSys.GetFunctionAnnotation(F);
      if (!annotation) {
        HLOpcodeGroup group = hlsl::GetHLOpcodeGroupByName(F);
        switch (group) {
        case HLOpcodeGroup::HLMatLoadStore: {
          HLMatLoadStoreOpcode opcode =
              static_cast<HLMatLoadStoreOpcode>(hlsl::GetHLOpcode(CI));
          switch (opcode) {
          case HLMatLoadStoreOpcode::ColMatLoad:
          case HLMatLoadStoreOpcode::RowMatLoad:
            PS.hasLoad = true;
            break;
          case HLMatLoadStoreOpcode::ColMatStore:
          case HLMatLoadStoreOpcode::RowMatStore:
            PS.hasStore = true;
            break;
          default:
            DXASSERT(0, "invalid opcode");
            PS.hasLoad = true;
            PS.hasStore = true;
            return;
          }
        } break;
        case HLOpcodeGroup::HLSubscript: {
          HLSubscriptOpcode opcode =
              static_cast<HLSubscriptOpcode>(hlsl::GetHLOpcode(CI));
          switch (opcode) {
          case HLSubscriptOpcode::VectorSubscript:
          case HLSubscriptOpcode::ColMatElement:
          case HLSubscriptOpcode::ColMatSubscript:
          case HLSubscriptOpcode::RowMatElement:
          case HLSubscriptOpcode::RowMatSubscript:
            analyzePointer(CI, PS, typeSys);
            break;
          default:
            // Rest are resource ptr like buf[i].
            // Only read of resource handle.
            PS.hasLoad = true;
            break;
          }
        } break;
        default: {
          // If not sure its out param or not. Take as out param.
          PS.hasLoad = true;
          PS.hasStore = true;
          return;
        }
        }
        continue;
      }

      unsigned argSize = F->arg_size();
      for (unsigned i = 0; i < argSize; i++) {
        Value *arg = CI->getArgOperand(i);
        if (V == arg) {
          // Do not replace struct arg.
          // Mark stored and loaded to disable replace.
          PS.hasLoad = true;
          PS.hasStore = true;
          return;
        }
      }
    }
  }
}

class HLLegalizeParameter : public ModulePass {
public:
  static char ID;
  explicit HLLegalizeParameter() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

private:
  void patchWriteOnInParam(Function &F, Argument &Arg, const DataLayout &DL);
  void patchReadOnOutParam(Function &F, Argument &Arg, const DataLayout &DL);
};

AllocaInst *createAllocaForPatch(Function &F, Argument &Arg) {
  IRBuilder<> Builder(F.getEntryBlock().getFirstInsertionPt());
  return Builder.CreateAlloca(Arg.getType()->getPointerElementType());
}

} // namespace

bool HLLegalizeParameter::runOnModule(Module &M) {
  HLModule &HLM = M.GetOrCreateHLModule();
  auto &typeSys = HLM.GetTypeSystem();
  const DataLayout &DL = M.getDataLayout();

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    DxilFunctionAnnotation *Annot = HLM.GetFunctionAnnotation(&F);
    if (!Annot)
      continue;

    for (Argument &Arg : F.args()) {
      if (!Arg.getType()->isPointerTy())
        continue;
      Type *EltTy = dxilutil::GetArrayEltTy(Arg.getType());
      if (dxilutil::IsHLSLObjectType(EltTy) ||
          dxilutil::IsHLSLResourceType(EltTy))
        continue;

      DxilParameterAnnotation &ParamAnnot =
          Annot->GetParameterAnnotation(Arg.getArgNo());
      switch (ParamAnnot.GetParamInputQual()) {
      default:
        break;
      case DxilParamInputQual::In:
      case DxilParamInputQual::InPayload: {
        PointerStatus PS;
        PS.analyzePointer(&Arg, PS, typeSys);
        if (PS.hasStore) {
          patchWriteOnInParam(F, Arg, DL);
        }
      } break;
      case DxilParamInputQual::Out: {
        PointerStatus PS;
        PS.analyzePointer(&Arg, PS, typeSys);
        if (PS.hasLoad) {
          patchReadOnOutParam(F, Arg, DL);
        }
      }
      }
    }
  }

  return true;
}


void HLLegalizeParameter::patchWriteOnInParam(Function &F, Argument &Arg,
                                              const DataLayout &DL) {
  AllocaInst *temp = createAllocaForPatch(F, Arg);
  Arg.replaceAllUsesWith(temp);
  IRBuilder<> Builder(temp->getNextNode());
  unsigned size = DL.getTypeAllocSize(Arg.getType()->getPointerElementType());
  // copy arg to temp at beginning of function.
  Builder.CreateMemCpy(temp, &Arg, size, 1);
}

void HLLegalizeParameter::patchReadOnOutParam(Function &F, Argument &Arg,
                                              const DataLayout &DL) {
  AllocaInst *temp = createAllocaForPatch(F, Arg);
  Arg.replaceAllUsesWith(temp);

  unsigned size = DL.getTypeAllocSize(Arg.getType()->getPointerElementType());
  for (auto &BB : F.getBasicBlockList()) {
    // copy temp to arg before every return.
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      IRBuilder<> RetBuilder(RI);
      RetBuilder.CreateMemCpy(&Arg, temp, size, 1);
    }
  }

}

char HLLegalizeParameter::ID = 0;
ModulePass *llvm::createHLLegalizeParameter() {
  return new HLLegalizeParameter();
}

INITIALIZE_PASS(HLLegalizeParameter, "hl-legalize-parameter",
                "Legalize parameter", false, false)
