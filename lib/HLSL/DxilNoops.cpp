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
#include <unordered_map>

using namespace llvm;

namespace {
StringRef kNoopName = "dx.noop";
StringRef kPreservePrefix = "dx.preserve.";
StringRef kNothingName = "dx.nothing";
StringRef kPreserveName = "dx.preserve.value";
}

bool hlsl::IsDxilPreserve(const Value *V) {
  if (const CallInst *CI = dyn_cast<CallInst>(V))
    if (Function *F = CI->getCalledFunction())
      return F->getName().startswith(kPreservePrefix);
  return false;
}

Value *hlsl::GetDxilPreserveSrc(Value *V) {
  assert(IsDxilPreserve(V));
  CallInst *CI = cast<CallInst>(V);
  return CI->getArgOperand(0);
}

static Function *GetOrCreatePreserveF(Module *M, Type *Ty) {
  std::string str = kPreservePrefix;
  raw_string_ostream os(str);
  Ty->print(os);
  os.flush();

  FunctionType *FT = FunctionType::get(Ty, { Ty, Ty }, false);
  Function *PreserveF = cast<Function>(M->getOrInsertFunction(str, FT));
  PreserveF->addFnAttr(Attribute::AttrKind::ReadNone);
  PreserveF->addFnAttr(Attribute::AttrKind::NoUnwind);
  return PreserveF;
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

static Value *GetSourcePointer(Value *V) {
  while (1) {
    if (GEPOperator *Gep = dyn_cast<GEPOperator>(V)) {
      V = Gep->getPointerOperand();
    }
    else if (isa<AllocaInst>(V) || isa<Argument>(V)) {
      return V;
    }
    else {
      break;
    }
  }
  return nullptr;
}

static bool PointerHasLoads(Value *Ptr) {
  SmallVector<Value *, 8> Worklist;
  for (User *U : Ptr->users()) {
    Worklist.push_back(U);
  }

  while (Worklist.size()) {
    Value *V = Worklist.pop_back_val();
    if (isa<GEPOperator>(V)) {
      for (User *U : V->users()) {
        Worklist.push_back(U);
      }
    }
    else if (isa<LoadInst>(V)) {
      return true;
    }
  }
  return false;
}

bool DxilInsertNoops::runOnFunction(Function &F) {
  Module &M = *F.getParent();
  Function *NoopF = nullptr;
  SmallDenseMap<Type *, Function *> PreserveFunctions;
  bool Changed = false;

  // Find instructions where we want to insert nops
  for (BasicBlock &BB : F) {
    for (BasicBlock::iterator It = BB.begin(), E = BB.end(); It != E;) {
      bool InsertNop = false;
      Value *CopySource = nullptr;
      Instruction &I = *(It++);
      Value *PrevValuePtr = nullptr;

      // If we are calling a real function, insert a nop
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
      // insert a preserve there.
      else if (StoreInst *Store = dyn_cast<StoreInst>(&I)) {
        Value *V = Store->getValueOperand();
        InsertNop = true;
        CopySource = V;
        PrevValuePtr = Store->getPointerOperand();
      }
      // If we have a return, insert a nop just to be safe.
      else if (ReturnInst *Ret = dyn_cast<ReturnInst>(&I)) {
        InsertNop = true;
      }

      // Do the insertion
      if (InsertNop) {
        // If we have a copy, insert a preserve instead. Only do it for
        // scalar and vector values for now.
        if (CopySource &&
          !CopySource->getType()->isAggregateType() &&
          !CopySource->getType()->isPointerTy())
        {
          Type *Ty = CopySource->getType();

          // Use the cached preserve function.
          Function *PreserveF = PreserveFunctions[Ty];
          if (!PreserveF) {
            PreserveF = GetOrCreatePreserveF(&M, Ty);
            PreserveFunctions[Ty] = PreserveF;
          }

          IRBuilder<> B(&I);
          Value *Last_Value = nullptr;
          Value *SourcePointer = nullptr;
          // If there's never any loads for this memory location,
          // don't generate a load.
          if ((SourcePointer = GetSourcePointer(PrevValuePtr)) &&
              PointerHasLoads(SourcePointer))
          {
            Last_Value = B.CreateLoad(PrevValuePtr);
          }
          else {
            Last_Value = UndefValue::get(CopySource->getType());
          }
          Instruction *Preserve = CallInst::Create(PreserveF, ArrayRef<Value *> { CopySource, Last_Value }, "", &I);
          Preserve->setDebugLoc(I.getDebugLoc());
          I.replaceUsesOfWith(CopySource, Preserve);
        }
        // Insert a noop
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
// Scalarize pass
//

namespace {

class DxilScalarizePreserves : public ModulePass {
public:
  static char ID;
  DxilScalarizePreserves() : ModulePass(ID) {
    initializeDxilScalarizePreservesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Scalarize Preserves"; }
};

char DxilScalarizePreserves::ID;
}

bool DxilScalarizePreserves::runOnModule(Module &M) {
  SmallVector<Function *, 4> Functions;
  for (Function &F : M) {
    if (F.isDeclaration() && F.getName().startswith(kPreservePrefix) &&
      F.getReturnType()->isVectorTy())
    {
      Functions.push_back(&F);
    }
  }

  std::unordered_map<Type *, Function *> PreserveFunctions;

  for (Function *F : Functions) {
    for (auto it = F->user_begin(); it != F->user_end();) {
      auto *U = *(it++);
      CallInst *CI = cast<CallInst>(U);

      Value *Src = CI->getArgOperand(0);
      Value *Secondary = CI->getArgOperand(1);
      VectorType *VTy = cast<VectorType>(Src->getType());
      Type *ElemTy = VTy->getScalarType();

      Function *PreserveF = PreserveFunctions[ElemTy];
      if (!PreserveF) {
        PreserveF = GetOrCreatePreserveF(F->getParent(), ElemTy);
        PreserveFunctions[ElemTy] = PreserveF;
      }

      Value *NewVectorValue = UndefValue::get(VTy);

      IRBuilder<> B(CI);
      for (unsigned i = 0; i < VTy->getVectorNumElements(); i++) {
        Value *Src_i = B.CreateExtractElement(Src, i);
        Value *Secondary_i = B.CreateExtractElement(Secondary, i);

        SmallVector<Value *, 2> Args;
        Args.push_back(Src_i);
        Args.push_back(Secondary_i);

        auto *NewPreserve = B.CreateCall(PreserveF, Args);
        NewVectorValue = B.CreateInsertElement(NewVectorValue, NewPreserve, i);
      }

      CI->replaceAllUsesWith(NewVectorValue);
      CI->eraseFromParent();
    }
    F->eraseFromParent();
  }

  return true;
}

Pass *llvm::createDxilScalarizePreservesPass() {
  return new DxilScalarizePreserves();
}

INITIALIZE_PASS(DxilScalarizePreserves, "dxil-scalarize-preserves", "Dxil Scalarize Preserves", false, false)

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

  bool LowerPreserves(Module &M);
  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Finalize Noops"; }
};

char DxilFinalizeNoops::ID;
}

bool DxilFinalizeNoops::LowerPreserves(Module &M) {
  SmallVector<Function *, 4> PreserveFunctions;

  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;
    if (F.getName().startswith(kPreservePrefix)) {
      PreserveFunctions.push_back(&F);
    }
  }

  bool Changed = false;

  struct Function_Context {
    Function *F;
    LoadInst *Load;
    std::map<Type *, Value *> Values;
  };

  std::map<Function *, Function_Context> Contexts;
  auto GetValue = [&](Function *F, Type *Ty) -> Value* {
    Function_Context &ctx = Contexts[F];
    if (!ctx.F) {
      ctx.F = F;
      BasicBlock *BB = &F->getEntryBlock();
      IRBuilder<> B(&BB->front());

      GlobalVariable *GV = M.getGlobalVariable(kPreserveName);
      if (!GV) {
        Type *i32Ty = B.getInt32Ty();
        GV = new GlobalVariable(M,
          i32Ty, true,
          llvm::GlobalValue::InternalLinkage,
          llvm::ConstantInt::get(i32Ty, 0), kPreserveName);
      }

      ctx.Load = B.CreateLoad(GV);
    }

    Value *&V = ctx.Values[Ty];
    if (!V) {
      IRBuilder<> B(ctx.Load->getNextNode());
      if (Ty->isIntegerTy()) {
        V = B.CreateTruncOrBitCast(ctx.Load, Ty);
      }
      else if (Ty->isFloatingPointTy()) {
        V = B.CreateSIToFP(ctx.Load, Ty);
      }
    }

    return V;
  };

  for (Function *PreserveF : PreserveFunctions) {
    for (auto UserIt = PreserveF->user_begin(); UserIt != PreserveF->user_end();) {
      User *U = *(UserIt++);
      CallInst *Preserve = cast<CallInst>(U);
      Function *F = Preserve->getParent()->getParent();

      IRBuilder<> B(Preserve->getNextNode());

      Value *Src = Preserve->getOperand(0);
      Type *Ty = Preserve->getType();

      if (Value *NopV = GetValue(F, Ty)) {
        Value *NewSrc = nullptr;
        if (Ty->isIntegerTy()) {
          NewSrc = B.CreateOr(Src, NopV);
        }
        else if (Ty->isFloatingPointTy()) {
          NewSrc = B.CreateFAdd(Src, NopV);
        }

        if (NewSrc) {
          if (Instruction *SrcI = dyn_cast<Instruction>(NewSrc)) {
            SrcI->setDebugLoc(Preserve->getDebugLoc());
          }
          Src = NewSrc;
        }
      }

      Preserve->replaceAllUsesWith(Src);
      Preserve->eraseFromParent();
    }

    assert(PreserveF->use_empty() && "dx.preserve calls must be all removed now");
    PreserveF->eraseFromParent();

    Changed = true;
  }

  return Changed;
}

// Replace all @dx.noop's with load @dx.nothing.value
bool DxilFinalizeNoops::runOnModule(Module &M) {

  bool Changed = false;

  Changed |= LowerPreserves(M);

  Function *NoopF = nullptr;
  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;
    if (F.getName() == kNoopName) {
      NoopF = &F;
    }
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

