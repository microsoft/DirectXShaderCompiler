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
// This pass inserts dx.noop and dx.preserve where we want
// to preserve line mapping or perserve some intermediate
// values.

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

  SmallDenseMap<Type *, Function *> PreserveFunctions;

  DxilInsertNoops() : FunctionPass(ID) {
    initializeDxilInsertNoopsPass(*PassRegistry::getPassRegistry());
  }


  Instruction *CreatePreserve(Value *V, Value *LastV, Instruction *InsertPt);
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

Value *GetOrCreatePreserveCond(Function *F) {
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
  IRBuilder<> B(&BB->front());

  LoadInst *Load = B.CreateLoad(GV);
  return B.CreateTrunc(Load, B.getInt1Ty());
};


Instruction *DxilInsertNoops::CreatePreserve(Value *V, Value *LastV, Instruction *InsertPt) {
  assert(V->getType() == LastV->getType());
#if 0
  Type *Ty = V->getType();
  // Use the cached preserve function.
  Function *PreserveF = PreserveFunctions[Ty];
  if (!PreserveF) {
    PreserveF = GetOrCreatePreserveF(InsertPt->getModule(), Ty);
    PreserveFunctions[Ty] = PreserveF;
  }

  return CallInst::Create(PreserveF, ArrayRef<Value *> { V, LastV }, "", InsertPt);
#else
  //Type *Ty = V->getType();

  if (isa<UndefValue>(LastV))
    LastV = V;

  return SelectInst::Create(GetOrCreatePreserveCond(InsertPt->getParent()->getParent()), LastV, V, "", InsertPt);

#endif
}

bool DxilInsertNoops::runOnFunction(Function &F) {
  Module &M = *F.getParent();
  Function *NoopF = nullptr;
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
        if (isa<LoadInst>(V) || isa<Constant>(V) || isa<Argument>(V)) {
          InsertNop = true;
          CopySource = V;
          PrevValuePtr = Store->getPointerOperand();
        }
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
            Last_Value = CopySource;
          }

          Instruction *Preserve = CreatePreserve(CopySource, Last_Value, &I);
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

static void CollectPreserveInstructions(Module &M, SmallVectorImpl<SelectInst *> &Results) {
  GlobalVariable *GV = M.getGlobalVariable(kPreserveName, true);
  if (GV) {
    for (User *U : GV->users()) {
      LoadInst *LI = cast<LoadInst>(U);
      assert(LI->user_begin() != LI->user_end() &&
        std::next(LI->user_begin()) == LI->user_end());
      Instruction *I = cast<Instruction>(*LI->user_begin());

      for (User *UU : I->users())
        Results.push_back(cast<SelectInst>(UU));
    }
  }
}

bool DxilFinalizeNoops::LowerPreserves(Module &M) {

  SmallVector<SelectInst *, 8> Preserves;
  CollectPreserveInstructions(M, Preserves);

  if (Preserves.empty())
    return false;

  for (SelectInst *P : Preserves) {
    Value *PrevV = P->getTrueValue();
    Value *CurV = P->getFalseValue();

    if (isa<UndefValue>(PrevV))
      P->setOperand(1, CurV);
  }

  return true;
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

