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
//
// Here is how dx.preserve and dx.noop work.
//
// For example, the following HLSL code:
//
//     float foo(float y) {
//        float x = 10;
//        x = 20;
//        x += y;
//        return x;
//     }
//
//     float main() : SV_Target {
//       float ret = foo(10);
//       return ret;
//     }
//
// Ordinarily, it gets lowered as:
//
//     dx.op.storeOutput(3.0)
//
// Intermediate steps at "x = 20;", "x += y;", "return x", and
// even the call to "foo()" are lost.
//
// But with with Preserve and Noop:
//
//     void call dx.noop()           // float ret = foo(10);
//       %y = dx.preserve(10.0, 10.0)  // argument: y=10
//       %x0 = dx.preserve(10.0, 10.0) // float x = 10;
//       %x1 = dx.preserve(20.0, %x0)  // x = 20;
//       %x2 = fadd %x1, %y            // x += y;
//       void call dx.noop()           // return x
//     %ret = dx.preserve(%x2, %x2)   // ret = returned from foo()
//     dx.op.storeOutput(%ret)
//
// All the intermediate transformations are visible and could be
// made inspectable in the debugger.
//
// The reason why dx.preserve takes 2 arguments is so that the previous
// value of a variable does not get cleaned up by DCE. For example:
//
//    float x = ...;
//    do_some_stuff_with(x);
//    do_some_other_stuff(); // At this point, x's last values
//                           // are dead and register allocators
//                           // are free to reuse its location during
//                           // call this code.
//                           // So until x is assigned a new value below
//                           // x could become unavailable.
//                           //
//                           // The second parameter in dx.preserve
//                           // keeps x's previous value alive.
//
//    x = ...; // Assign something else
//
//
// When emitting proper DXIL, dx.noop and dx.preserve are lowered to
// ordinary LLVM instructions that do not affect the semantic of the
// shader, but can be used by a debugger or backend generator if they
// know what to look for.
//
// We generate two special internal constant global vars:
//
//      @dx.preserve.value = internal constant i1 false
//      @dx.nothing = internal constant i32 0
//
// "call dx.noop()" is lowered to "load @dx.nothing"
//
// "... = call dx.preserve(%cur_val, %last_val)" is lowered to:
//
//    %p = load @dx.preserve.value
//    ... = select i1 %p, %last_val, %cur_val
//
// Since %p is guaranteed to be false, the select is guaranteed
// to return %cur_val.
//

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_os_ostream.h"

#include <unordered_map>

using namespace llvm;

namespace {
StringRef kNoopName = "dx.noop";
StringRef kPreservePrefix = "dx.preserve.";
StringRef kNothingName = "dx.nothing";
StringRef kPreserveName = "dx.preserve.value";
}

static Function *GetOrCreateNoopF(Module &M) {
  LLVMContext &Ctx = M.getContext();
  FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), false);
  Function *NoopF = cast<Function>(M.getOrInsertFunction(::kNoopName, FT));
  NoopF->addFnAttr(Attribute::AttrKind::Convergent);
  return NoopF;
}

struct Store_Info {
  StoreInst *Store = nullptr;
  Value *Source = nullptr; // Alloca, GV, or Argument
  bool HasLoads = false;
};

static void FindAllStores(Value *Ptr, Function *F, SmallVectorImpl<Store_Info> *Stores, SmallVectorImpl<Value *> &WorklistStorage) {
  assert(isa<Argument>(Ptr) || isa<AllocaInst>(Ptr) || isa<GlobalVariable>(Ptr));

  WorklistStorage.clear();
  WorklistStorage.push_back(Ptr);

  bool HasLoad = false;
  while (WorklistStorage.size()) {
    Value *V = WorklistStorage.pop_back_val();

    if (Instruction *I = dyn_cast<Instruction>(V))
      if (I->getParent()->getParent() != F)
        continue;

    if (isa<GEPOperator>(V) || isa<GlobalVariable>(V) || isa<AllocaInst>(V)) {
      for (User *U : V->users())
        WorklistStorage.push_back(U);
    }
    else if (StoreInst *Store = dyn_cast<StoreInst>(V)) {
      Stores->push_back({ Store, Ptr });
    }
    else if (isa<LoadInst>(V)) {
      HasLoad = true;
    }
  }

  if (HasLoad)
    for (Store_Info &Info : *Stores)
      Info.HasLoads = true;
}

static Value *GetOrCreatePreserveCond(Function *F) {
  assert(!F->isDeclaration());

  Module *M = F->getParent();
  GlobalVariable *GV = M->getGlobalVariable(kPreserveName, true);
  if (!GV) {
    Type *i32Ty = Type::getInt32Ty(M->getContext());
    GV = new GlobalVariable(*M,
      i32Ty, true,
      llvm::GlobalValue::InternalLinkage,
      llvm::ConstantInt::get(i32Ty, 0), kPreserveName);
  }

  for (User *U : GV->users()) {
    LoadInst *LI = cast<LoadInst>(U);
    if (LI->getParent()->getParent() == F) {
      assert(LI->user_begin() != LI->user_end() &&
        std::next(LI->user_begin()) == LI->user_end());

      return *LI->user_begin();
    }
  }

  BasicBlock *BB = &F->getEntryBlock();
  Instruction *InsertPt = &BB->front();
  while (isa<AllocaInst>(InsertPt) || isa<DbgInfoIntrinsic>(InsertPt))
    InsertPt = InsertPt->getNextNode();

  IRBuilder<> B(InsertPt);

  LoadInst *Load = B.CreateLoad(GV);
  return B.CreateTrunc(Load, B.getInt1Ty());
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

static Instruction *CreatePreserve(Value *V, Value *LastV, Instruction *InsertPt) {
  assert(V->getType() == LastV->getType());
  Type *Ty = V->getType();
  // Use the cached preserve function.
  Function *PreserveF = GetOrCreatePreserveF(InsertPt->getModule(), Ty);
  return CallInst::Create(PreserveF, ArrayRef<Value *> { V, LastV }, "", InsertPt);
}

static void LowerPreserveToSelect(CallInst *CI) {
  Value *V = CI->getArgOperand(0);
  Value *LastV = CI->getArgOperand(1);

  if (LastV == V)
    LastV = UndefValue::get(V->getType());

  Value *Cond = GetOrCreatePreserveCond(CI->getParent()->getParent());
  SelectInst *Select = SelectInst::Create(Cond, LastV, V, "", CI);
  Select->setDebugLoc(CI->getDebugLoc());
  CI->replaceAllUsesWith(Select);
  CI->eraseFromParent();
}


//==========================================================
// Insertion pass
//
// This pass inserts dx.noop and dx.preserve where we want
// to preserve line mapping or perserve some intermediate
// values.

struct DxilInsertPreserves : public ModulePass {
  static char ID;
  DxilInsertPreserves() : ModulePass(ID) {
    initializeDxilInsertPreservesPass(*PassRegistry::getPassRegistry());
  }

  bool DoFunction(Function &F) {
    if (F.empty())
      return false;
    BasicBlock *Entry = &*F.begin();

    SmallVector<Store_Info, 10> Stores;
    SmallVector<Value *, 10> WorklistStorage;
    for (Instruction &I : *Entry) {
      AllocaInst *AI = dyn_cast<AllocaInst>(&I);
      if (!AI)
        break;

      FindAllStores(AI, &F, &Stores, WorklistStorage);
    }

    Module *M = F.getParent();
    for (GlobalVariable &GV : M->globals()) {
      FindAllStores(&GV, &F, &Stores, WorklistStorage);
    }

    bool Changed = false;
    for (Store_Info &Info : Stores) {
      StoreInst *Store = Info.Store;
      Value *V = Store->getValueOperand();
      Value *CopySource = nullptr;

      if (isa<LoadInst>(V) || isa<Constant>(V) || isa<CallInst>(V)) {
        CopySource = V;
      }

      if (CopySource &&
        !CopySource->getType()->isAggregateType() &&
        !CopySource->getType()->isPointerTy())
      {
        IRBuilder<> B(Store);
        Value *Last_Value = nullptr;
        // If there's never any loads for this memory location,
        // don't generate a load.
        if (Info.HasLoads) {
          Last_Value = B.CreateLoad(Store->getPointerOperand());
        }
        else {
          Last_Value = UndefValue::get(CopySource->getType());
        }

        Instruction *Preserve = CreatePreserve(CopySource, Last_Value, Store);
        Preserve->setDebugLoc(Store->getDebugLoc());
        Store->replaceUsesOfWith(CopySource, Preserve);

        Changed = true;
      }
      else {
        Function *NoopF = GetOrCreateNoopF(*M);
        CallInst *Noop = CallInst::Create(NoopF, {}, Store);
        Noop->setDebugLoc(Store->getDebugLoc());
      }
    }

    return Changed;
  }

  bool runOnModule(Module &M) override {

    bool Changed = false;
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      for (User *U : F.users()) {
        if (CallInst *CI = dyn_cast<CallInst>(U)) {
          Function *NoopF = GetOrCreateNoopF(M);
          CallInst *Noop = CallInst::Create(NoopF, {}, CI);
          Noop->setDebugLoc(CI->getDebugLoc());
          Changed = true;
        }
      }

      Changed |= DoFunction(F);

    }

    return Changed;
  }

  const char *getPassName() const override { return "Dxil Insert Preserves"; }
};

char DxilInsertPreserves::ID;

Pass *llvm::createDxilInsertPreservesPass() {
  return new DxilInsertPreserves();
}

INITIALIZE_PASS(DxilInsertPreserves, "dxil-insert-preserves", "Dxil Insert Preserves", false, false)


//==========================================================
// Lower dx.preserve to select
//
// This pass replaces all dx.preserve calls to select
//

namespace {

class DxilPreserveToSelect : public ModulePass {
public:
  static char ID;

  SmallDenseMap<Type *, Function *> PreserveFunctions;

  DxilPreserveToSelect() : ModulePass(ID) {
    initializeDxilPreserveToSelectPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    bool Changed = false;
    for (auto fit = M.getFunctionList().begin(), end = M.getFunctionList().end();
      fit != end;)
    {
      Function *F = &*(fit++);
      if (!F->isDeclaration())
        continue;

      if (F->getName().startswith(kPreservePrefix)) {
        for (auto uit = F->user_begin(), end = F->user_end(); uit != end;) {
          User *U = *(uit++);
          CallInst *CI = cast<CallInst>(U);
          LowerPreserveToSelect(CI);
        }

        F->eraseFromParent();
        Changed = true;
      }
    }

    return Changed;
  }
  const char *getPassName() const override { return "Dxil Lower Preserves to Selects"; }
};

char DxilPreserveToSelect::ID;
}

Pass *llvm::createDxilPreserveToSelectPass() {
  return new DxilPreserveToSelect();
}

INITIALIZE_PASS(DxilPreserveToSelect, "dxil-insert-noops", "Dxil Insert Noops", false, false)


//==========================================================
// Finalize pass
//

namespace {

class DxilFinalizePreserves : public ModulePass {
public:
  static char ID;
  GlobalVariable *NothingGV = nullptr;

  DxilFinalizePreserves() : ModulePass(ID) {
    initializeDxilFinalizePreservesPass(*PassRegistry::getPassRegistry());
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
  bool LowerNoops(Module &M);
  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Finalize Preserves"; }
};

char DxilFinalizePreserves::ID;
}

// Fix undefs in the dx.preserve -> selects
bool DxilFinalizePreserves::LowerPreserves(Module &M) {
  bool Changed = false;

  GlobalVariable *GV = M.getGlobalVariable(kPreserveName, true);
  if (GV) {
    for (User *U : GV->users()) {
      LoadInst *LI = cast<LoadInst>(U);
      assert(LI->user_begin() != LI->user_end() &&
        std::next(LI->user_begin()) == LI->user_end());
      Instruction *I = cast<Instruction>(*LI->user_begin());

      for (User *UU : I->users()) {

        SelectInst *P = cast<SelectInst>(UU);
        Value *PrevV = P->getTrueValue();
        Value *CurV = P->getFalseValue();

        if (isa<UndefValue>(PrevV)) {
          P->setOperand(1, CurV);
          Changed = true;
        }
      }
    }
  }

  return Changed;
}

// Replace all @dx.noop's with load @dx.nothing.value
bool DxilFinalizePreserves::LowerNoops(Module &M) {
  bool Changed = false;

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

// Replace all preserves and nops
bool DxilFinalizePreserves::runOnModule(Module &M) {
  bool Changed = false;

  Changed |= LowerPreserves(M);
  Changed |= LowerNoops(M);

  return Changed;
}

Pass *llvm::createDxilFinalizePreservesPass() {
  return new DxilFinalizePreserves();
}

INITIALIZE_PASS(DxilFinalizePreserves, "dxil-finalize-preserves", "Dxil Finalize Preserves", false, false)

