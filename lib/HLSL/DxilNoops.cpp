///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilNoops.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Passes to insert dx.noops() and replace them with llvm.donothing()        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;

namespace {
StringRef kNoopName = "dx.noop";
StringRef kNothingName = "dx.nothing";
}

//==========================================================
// Insertion pass
//

namespace {

Function *GetOrCreateNoopF(Module &M) {
  LLVMContext &Ctx = M.getContext();
  FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), false);
  Function *NoopF = cast<Function>(M.getOrInsertFunction(::kNoopName, FT));
  NoopF->addFnAttr(Attribute::AttrKind::Convergent);
  return NoopF;
}

class DxilInsertNoops : public FunctionPass {
public:
  static char ID;
  DxilInsertNoops() : FunctionPass(ID) {
    initializeDxilInsertNoopsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;
  const char *getPassName() const override { return "Dxil Insert Noops"; }
};

char DxilInsertNoops::ID;
}

bool DxilInsertNoops::runOnFunction(Function &F) {
  Module &M = *F.getParent();
  Function *NoopF = nullptr;
  bool Changed = false;

  // Find instructions where we want to insert nops
  for (BasicBlock &BB : F) {
    for (BasicBlock::iterator It = BB.begin(), E = BB.end(); It != E;) {
      bool InsertNop = false;
      Instruction &I = *(It++);
      // If we are calling a real function, insert one
      // at the callsite.
      if (CallInst *Call = dyn_cast<CallInst>(&I)) {
        if (Function *F = Call->getCalledFunction()) {
          if (!F->isDeclaration())
            InsertNop = true;
        }
      }
      else if (MemCpyInst *MC = dyn_cast<MemCpyInst>(&I)) {
        InsertNop = true;
      }
      // If we have a copy, e.g:
      //     float x = 0;
      //     float y = x;    <---- copy
      // insert a nop there.
      else if (StoreInst *Store = dyn_cast<StoreInst>(&I)) {
        Value *V = Store->getValueOperand();
        if (isa<LoadInst>(V) || isa<Constant>(V))
          InsertNop = true;
      }
      // If we have a return, just to be safe.
      else if (ReturnInst *Ret = dyn_cast<ReturnInst>(&I)) {
        InsertNop = true;
      }

      // Do the insertion
      if (InsertNop) {
        if (!NoopF) 
          NoopF = GetOrCreateNoopF(M);
        CallInst *Noop = CallInst::Create(NoopF, {}, &I);
        Noop->setDebugLoc(I.getDebugLoc());
        Changed = true;
      }
    }
  }

  return Changed;
}

Pass *llvm::createDxilInsertNoopsPass() {
  return new DxilInsertNoops();
}

INITIALIZE_PASS(DxilInsertNoops, "dxil-insert-noops", "Dxil Insert Noops", false, false)


//==========================================================
// Finalize pass
//

namespace {

class DxilFinalizeNoops : public ModulePass {
public:
  static char ID;
  GlobalVariable *NothingGV = nullptr;

  DxilFinalizeNoops() : ModulePass(ID) {
    initializeDxilFinalizeNoopsPass(*PassRegistry::getPassRegistry());
  }

  Instruction *GetFinalNoopInst(Module &M, Instruction *InsertBefore) {
  if (!NothingGV) {
    NothingGV = M.getGlobalVariable(kNothingName);
    if (!NothingGV) {
      Type *i32Ty = Type::getInt32Ty(M.getContext());
      NothingGV = new GlobalVariable(M,
        i32Ty, true,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(i32Ty, 0), kNothingName);
    }
  }

  return new llvm::LoadInst(NothingGV, nullptr, InsertBefore);
}

  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Finalize Noops"; }
};

char DxilFinalizeNoops::ID;
}

// Replace all @dx.noop's with @llvm.donothing
bool DxilFinalizeNoops::runOnModule(Module &M) {
  Function *NoopF = nullptr;
  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;
    if (F.getName() == ::kNoopName) {
      NoopF = &F;
      break;
    }
  }

  if (!NoopF)
    return false;

  if (!NoopF->user_empty()) {
    for (auto It = NoopF->user_begin(), E = NoopF->user_end(); It != E;) {
      User *U = *(It++);
      CallInst *CI = cast<CallInst>(U);

      Instruction *Nop = GetFinalNoopInst(M, CI);
      Nop->setDebugLoc(CI->getDebugLoc());

      CI->eraseFromParent();
    }
  }

  assert(NoopF->user_empty() && "dx.noop calls must be all removed now");
  NoopF->eraseFromParent();

  return true;
}

Pass *llvm::createDxilFinalizeNoopsPass() {
  return new DxilFinalizeNoops();
}

INITIALIZE_PASS(DxilFinalizeNoops, "dxil-finalize-noops", "Dxil Finalize Noops", false, false)

