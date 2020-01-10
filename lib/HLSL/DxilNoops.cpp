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
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_os_ostream.h"

#include "dxc/HLSL/DxilNoops.h"

using namespace llvm;

namespace {
StringRef kNoopName = "dx.noop";
StringRef kCopyPrefix = "dx.copy.";
StringRef kNothingName = "dx.nothing";
}

bool hlsl::IsDxilCopy(CallInst *CI) {
  if (Function *F = CI->getCalledFunction())
    return F->getName().startswith(kCopyPrefix);
  return false;
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
  SmallDenseMap<Type *, Function *> CopyFunctions;
  bool Changed = false;

  // Find instructions where we want to insert nops
  for (BasicBlock &BB : F) {
    for (BasicBlock::iterator It = BB.begin(), E = BB.end(); It != E;) {
      bool InsertNop = false;
      Value *CopySource = nullptr;
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
        if (isa<LoadInst>(V)) {
          InsertNop = true;
          CopySource = V;
        }
        else if (isa<Constant>(V)) {
          InsertNop = true;
        }
      }
      // If we have a return, just to be safe.
      else if (ReturnInst *Ret = dyn_cast<ReturnInst>(&I)) {
        InsertNop = true;
      }

      // Do the insertion
      if (InsertNop) {
        if (CopySource &&
          !CopySource->getType()->isAggregateType() &&
          !CopySource->getType()->isPointerTy())
        {
          Type *Ty = CopySource->getType()->getScalarType();

          Function *CopyF = CopyFunctions[Ty];
          if (!CopyF) {
            std::string str = kCopyPrefix;
            raw_string_ostream os(str);
            Ty->print(os);
            os.flush();

            FunctionType *FT = FunctionType::get(Ty, Ty, false);
            CopyF = cast<Function>(M.getOrInsertFunction(str, FT));
            CopyF->addFnAttr(Attribute::AttrKind::ReadNone);
            CopyF->addFnAttr(Attribute::AttrKind::Convergent);
            CopyFunctions[Ty] = CopyF;
          }

          auto MarkCopy = [&I, CopyF](Value *Src) {
            CallInst *Copy = CallInst::Create(CopyF, ArrayRef<Value *> {Src}, "copy", &I);
            Copy->setDebugLoc(I.getDebugLoc());
            return Copy;
          };

          if (CopySource->getType()->isVectorTy()) {
            Type *VecTy = CopySource->getType();

            SmallVector<Value *, 4> Elements;
            IRBuilder<> B(&I);
            for (unsigned i = 0; i < VecTy->getVectorNumElements(); i++) {
              auto *EE = B.CreateExtractElement(CopySource, i);
              Instruction *Copy = CallInst::Create(CopyF, ArrayRef<Value *> {EE}, "copy", &I);
              Copy->setDebugLoc(I.getDebugLoc());
              Elements.push_back(Copy);
            }

            Value *Vec = UndefValue::get(VecTy);
            for (unsigned i = 0; i < Elements.size(); i++) {
              Vec = B.CreateInsertElement(Vec, Elements[i], i);
            }

            I.replaceUsesOfWith(CopySource, Vec);
          }
          else {
            Instruction *Copy = CallInst::Create(CopyF, ArrayRef<Value *> { CopySource }, "copy", &I);
            Copy->setDebugLoc(I.getDebugLoc());
            I.replaceUsesOfWith(CopySource, Copy);
          }
        }
        else {
          if (!NoopF)
            NoopF = GetOrCreateNoopF(M);
          CallInst *Noop = CallInst::Create(NoopF, {}, &I);
          Noop->setDebugLoc(I.getDebugLoc());
        }
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

  return new llvm::LoadInst(NothingGV, "nop", InsertBefore);
}

  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Finalize Noops"; }
};

char DxilFinalizeNoops::ID;
}

// Replace all @dx.noop's with @llvm.donothing
bool DxilFinalizeNoops::runOnModule(Module &M) {
  Function *NoopF = nullptr;
  SmallVector<Function *, 4> CopyFunctions;

  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;
    if (F.getName() == ::kNoopName) {
      NoopF = &F;
    }
    else if (F.getName().startswith(kCopyPrefix)) {
      CopyFunctions.push_back(&F);
    }
  }

  bool Changed = false;

  for (Function *CopyF : CopyFunctions) {
    for (User *U : CopyF->users()) {
      CallInst *Copy = cast<CallInst>(U);

      Instruction *Nop = GetFinalNoopInst(M, Copy);
      Nop->setDebugLoc(Copy->getDebugLoc());

      IRBuilder<> B(Nop->getNextNode());

      Value *Src = Copy->getOperand(0);
      Type *Ty = Copy->getType();
      if (Ty->isIntegerTy()) {
        Value *NopV = B.CreateTruncOrBitCast(Nop, Copy->getType());
        Src = B.CreateOr(NopV, Src);
      }
      else if (Ty->isFloatingPointTy()) {
        Value *NopV = B.CreateSIToFP(Nop, Copy->getType());
        Src = B.CreateFAdd(Src, NopV);
      }

      Copy->replaceAllUsesWith(Src);
      Copy->eraseFromParent();
    }

    assert(CopyF->use_empty() && "dx.copy calls must be all removed now");
    CopyF->eraseFromParent();

    Changed = true;
  }

  if (NoopF) {
    for (auto It = NoopF->user_begin(), E = NoopF->user_end(); It != E;) {
      User *U = *(It++);
      CallInst *CI = cast<CallInst>(U);

      Instruction *Nop = GetFinalNoopInst(M, CI);
      Nop->setDebugLoc(CI->getDebugLoc());

      CI->eraseFromParent();
      Changed = true;
    }

    assert(NoopF->user_empty() && "dx.noop calls must be all removed now");
    NoopF->eraseFromParent();

  }

  return Changed;
}

Pass *llvm::createDxilFinalizeNoopsPass() {
  return new DxilFinalizeNoops();
}

INITIALIZE_PASS(DxilFinalizeNoops, "dxil-finalize-noops", "Dxil Finalize Noops", false, false)

