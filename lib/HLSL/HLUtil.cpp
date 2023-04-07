///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLUtil.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// HL helper functions.                                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/HLUtil.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"

#include "dxc/Support/Global.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"

using namespace llvm;
using namespace hlsl;
using namespace hlsl::hlutil;

namespace {
void analyzePointer(const Value *V, PointerStatus &PS, DxilTypeSystem &typeSys,
                    bool bStructElt, bool bLdStOnly) {
  // Early return when only care load store.
  if (bLdStOnly) {
    if (PS.HasLoaded() && PS.HasStored())
       return;
  }
  for (const User *U : V->users()) {
    if (const Instruction *I = dyn_cast<Instruction>(U)) {
      const Function *F = I->getParent()->getParent();
      if (!PS.AccessingFunction) {
        PS.AccessingFunction = F;
      } else {
        if (F != PS.AccessingFunction)
          PS.HasMultipleAccessingFunctions = true;
      }
    }

    if (const BitCastOperator *BC = dyn_cast<BitCastOperator>(U)) {
      analyzePointer(BC, PS, typeSys, bStructElt, bLdStOnly);
    } else if (const MemCpyInst *MC = dyn_cast<MemCpyInst>(U)) {
      // Do not collect memcpy on struct GEP use.
      // These memcpy will be flattened in next level.
      if (!bStructElt) {
        MemCpyInst *MI = const_cast<MemCpyInst *>(MC);
        PS.memcpySet.insert(MI);
        bool bFullCopy = false;
        if (ConstantInt *Length = dyn_cast<ConstantInt>(MC->getLength())) {
          bFullCopy = PS.Size == Length->getLimitedValue() || PS.Size == 0 ||
                      Length->getLimitedValue() == 0; // handle unbounded arrays
        }
        if (MC->getRawDest() == V) {
          if (bFullCopy &&
              PS.storedType == PointerStatus::StoredType::NotStored) {
            PS.storedType = PointerStatus::StoredType::MemcopyDestOnce;
            PS.StoringMemcpy = MI;
          } else {
            PS.MarkAsStored();
            PS.StoringMemcpy = nullptr;
          }
        } else if (MC->getRawSource() == V) {
          if (bFullCopy &&
              PS.loadedType == PointerStatus::LoadedType::NotLoaded) {
            PS.loadedType = PointerStatus::LoadedType::MemcopySrcOnce;
            PS.LoadingMemcpy = MI;
          } else {
            PS.MarkAsLoaded();
            PS.LoadingMemcpy = nullptr;
          }
        }
      } else {
        if (MC->getRawDest() == V) {
          PS.MarkAsStored();
        } else {
          DXASSERT(MC->getRawSource() == V, "must be source here");
          PS.MarkAsLoaded();
        }
      }
    } else if (const GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      gep_type_iterator GEPIt = gep_type_begin(GEP);
      gep_type_iterator GEPEnd = gep_type_end(GEP);
      // Skip pointer idx.
      GEPIt++;
      // Struct elt will be flattened in next level.
      bool bStructElt = (GEPIt != GEPEnd) && GEPIt->isStructTy();
      analyzePointer(GEP, PS, typeSys, bStructElt, bLdStOnly);
    } else if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
      Value *V = SI->getOperand(0);

      if (PS.storedType == PointerStatus::StoredType::NotStored) {
        PS.storedType = PointerStatus::StoredType::StoredOnce;
        PS.StoredOnceValue = V;
      } else {
        PS.MarkAsStored();
      }
    } else if (dyn_cast<LoadInst>(U)) {
      PS.MarkAsLoaded();
    } else if (const CallInst *CI = dyn_cast<CallInst>(U)) {
      Function *F = CI->getCalledFunction();
      if (F->isIntrinsic()) {
        if (F->getIntrinsicID() == Intrinsic::lifetime_start ||
            F->getIntrinsicID() == Intrinsic::lifetime_end)
          continue;
      }
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
            PS.MarkAsLoaded();
            break;
          case HLMatLoadStoreOpcode::ColMatStore:
          case HLMatLoadStoreOpcode::RowMatStore:
            PS.MarkAsStored();
            break;
          default:
            DXASSERT(0, "invalid opcode");
            PS.MarkAsStored();
            PS.MarkAsLoaded();
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
            analyzePointer(CI, PS, typeSys, bStructElt, bLdStOnly);
            break;
          default:
            // Rest are resource ptr like buf[i].
            // Only read of resource handle.
            PS.MarkAsLoaded();
            break;
          }
        } break;
        default: {
          // If not sure its out param or not. Take as out param.
          PS.MarkAsStored();
          PS.MarkAsLoaded();
        }
        }
        continue;
      }

      unsigned argSize = F->arg_size();
      for (unsigned i = 0; i < argSize; i++) {
        Value *arg = CI->getArgOperand(i);
        if (V == arg) {
          if (bLdStOnly) {
            auto &paramAnnot = annotation->GetParameterAnnotation(i);
            switch (paramAnnot.GetParamInputQual()) {
            default:
              PS.MarkAsStored();
              PS.MarkAsLoaded();
              break;
            case DxilParamInputQual::Out:
              PS.MarkAsStored();
              break;
            case DxilParamInputQual::In:
              PS.MarkAsLoaded();
              break;
            }
          } else {
            // Do not replace struct arg.
            // Mark stored and loaded to disable replace.
            PS.MarkAsStored();
            PS.MarkAsLoaded();
          }
        }
      }
    }
  }
}
}

namespace hlsl {
namespace hlutil {

void PointerStatus::analyze(DxilTypeSystem &typeSys, bool bStructElt) {
  analyzePointer(Ptr, *this, typeSys, bStructElt, bLoadStoreOnly);
}

PointerStatus::PointerStatus(llvm::Value *ptr, unsigned size, bool bLdStOnly)
    : storedType(StoredType::NotStored), loadedType(LoadedType::NotLoaded),
      StoredOnceValue(nullptr), StoringMemcpy(nullptr), LoadingMemcpy(nullptr),
      AccessingFunction(nullptr), HasMultipleAccessingFunctions(false),
      Size(size), Ptr(ptr), bLoadStoreOnly(bLdStOnly) {}

void PointerStatus::MarkAsStored() {
  storedType = StoredType::Stored;
  StoredOnceValue = nullptr;
}
void PointerStatus::MarkAsLoaded() { loadedType = LoadedType::Loaded; }
bool PointerStatus::HasStored() {
  return storedType != StoredType::NotStored &&
         storedType != StoredType::InitializerStored;
}
bool PointerStatus::HasLoaded() { return loadedType != LoadedType::NotLoaded; }

} // namespace hlutil
} // namespace hlsl

namespace {

// Mutate key types in mutateTypeMap to value types.
struct TypeMutator {
  TypeMutator(MutateTypeFunction MutateTyFn, DxilTypeSystem &TS)
      : MutateTyFn(MutateTyFn), TS(TS) {}

  Type *mutateType(Type *Ty) {
    auto it = MutateTypeMap.find(Ty);
    if (it != MutateTypeMap.end())
      return it->second;


    Type *ResultTy = nullptr;
    if (Type *MT = MutateTyFn(Ty)) {
      ResultTy = MT;
    } else if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
      SmallVector<unsigned, 2> nestedSize;
      Type *EltTy = Ty;
      while (ArrayType *NestAT = dyn_cast<ArrayType>(EltTy)) {
        nestedSize.emplace_back(NestAT->getNumElements());
        EltTy = NestAT->getElementType();
      }
      Type *mutatedTy = mutateType(EltTy);
      if (mutatedTy == EltTy) {
        ResultTy = Ty;
      } else {
        Type *newAT = mutatedTy;
        for (auto it = nestedSize.rbegin(), E = nestedSize.rend(); it != E;
             ++it)
          newAT = ArrayType::get(newAT, *it);
        ResultTy = newAT;
      }
    } else if (PointerType *PT = dyn_cast<PointerType>(Ty)) {
      Type *EltTy = PT->getElementType();
      Type *mutatedTy = mutateType(EltTy);
      if (mutatedTy == EltTy)
        ResultTy = Ty;
      else
        ResultTy = mutatedTy->getPointerTo(PT->getAddressSpace());
    } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
      if (!ST->isOpaque()) {
        SmallVector<Type *, 4> Elts(ST->element_begin(), ST->element_end());
        if (!mutateTypes(Elts)) {
          ResultTy = Ty;
        } else {
          StringRef Name = ST->getName();
          ST->setName("");
          ResultTy = StructType::create(Elts, Name);
          if (DxilStructAnnotation *Annot = TS.GetStructAnnotation(ST)) {
            DxilStructAnnotation *NewAnnot = TS.AddStructAnnotation(
                (StructType *)ResultTy, Annot->GetNumTemplateArgs());
            for (unsigned i = 0; i < Annot->GetNumFields(); ++i) {
              NewAnnot->GetFieldAnnotation(i) = Annot->GetFieldAnnotation(i);
            }
            for (unsigned i = 0; i < Annot->GetNumTemplateArgs(); ++i) {
              NewAnnot->GetTemplateArgAnnotation(i) =
                  Annot->GetTemplateArgAnnotation(i);
            }
            NewAnnot->SetCBufferSize(Annot->GetCBufferSize());
            TS.FinishStructAnnotation(*NewAnnot);
          }
        }
      } else {
        ResultTy = Ty;
      }
    } else if (FunctionType *FT = dyn_cast<FunctionType>(Ty)) {
      Type *RetTy = FT->getReturnType();
      SmallVector<Type *, 4> Args(FT->param_begin(), FT->param_end());
      Type *mutatedRetTy = mutateType(RetTy);
      if (!mutateTypes(Args) && RetTy == mutatedRetTy) {
        ResultTy = Ty;
      } else {
        ResultTy = FunctionType::get(mutatedRetTy, Args, FT->isVarArg());
      }
    } else {
      ResultTy = Ty;
    }

    MutateTypeMap[Ty] = ResultTy;
    return ResultTy;
  }
  bool needMutate(Type *Ty) { return Ty != mutateType(Ty); }
  DxilTypeSystem &getTypeSys() { return TS; }

private:
  bool mutateTypes(SmallVector<Type *, 4> &Tys) {
    bool bMutated = false;
    for (size_t i = 0; i < Tys.size(); i++) {
      Type *Ty = Tys[i];
      Type *mutatedTy = mutateType(Ty);
      if (Ty != mutatedTy) {
        Tys[i] = mutatedTy;
        bMutated = true;
      }
    }
    return bMutated;
  }
  DenseMap<Type *, Type *> MutateTypeMap;
  MutateTypeFunction MutateTyFn;
  DxilTypeSystem &TS;
};

struct TypeUserMutator {
  TypeUserMutator(TypeMutator &TM, Module &M) : TM(TM), M(M) {}
  bool run();

private:
  void collectAlloca(Function &F, SmallVector<Value *, 8> &WorkList);
  void collectBitcast(AllocaInst *AI, SmallVector<Value *, 8> &WorkList);
  SmallVector<Value *, 8> collectBasicCandidates(Module &M);
  void collectCandidates();
  void mutateCandidates();

  DenseSet<Value *> MutateValSet;
  TypeMutator &TM;
  Module &M;
};

void TypeUserMutator::collectAlloca(Function &F,
                                    SmallVector<Value *, 8> &WorkList) {
  if (F.isDeclaration())
    return;
  for (Instruction &I : F.getEntryBlock()) {
    AllocaInst *AI = dyn_cast<AllocaInst>(&I);
    if (!AI)
      continue;
    collectBitcast(AI, WorkList);
    Type *Ty = AI->getType();
    if (!TM.needMutate(Ty))
      continue;
    WorkList.emplace_back(AI);
  }
}

// Collect bitcast to mutateType like
// %3 = alloca <12 x float>, align 8
// %4 = bitcast<12 x float> * % 2 to % class.matrix.float .3.4 *
void TypeUserMutator::collectBitcast(AllocaInst *AI,
                                    SmallVector<Value *, 8> &WorkList) {
  for (User *U : AI->users()) {
    Type *Ty = U->getType();
    if (!TM.needMutate(Ty))
      continue;
    WorkList.emplace_back(U);
  }
}

SmallVector<Value *, 8> TypeUserMutator::collectBasicCandidates(Module &M) {
  // Add all global/function/argument/alloca has candidate type.
  SmallVector<Value *, 8> WorkList;

  // Functions.
  for (Function &F : M) {
    // Collect alloca.
    collectAlloca(F, WorkList);
    FunctionType *FT = F.getFunctionType();
    if (!TM.needMutate(FT))
      continue;

    WorkList.emplace_back(&F);
    // Check args.
    for (Argument &Arg : F.args()) {
      Type *Ty = Arg.getType();
      if (!TM.needMutate(Ty))
        continue;

      WorkList.emplace_back(&Arg);
    }
    if (TM.needMutate(FT->getReturnType())) {
      // Add all returns.
      for (auto &BB : F) {
        ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator());
        if (!RI)
          continue;
        WorkList.emplace_back(RI);
      }
    }
  }

  // Static globals.
  for (GlobalVariable &GV : M.globals()) {
    Type *Ty = GV.getValueType();
    if (!TM.needMutate(Ty))
      continue;

    WorkList.emplace_back(&GV);
  }

  return WorkList;
}

void TypeUserMutator::collectCandidates() {
  // Functions, globals, allocas.
  // What else?
  SmallVector<Value *, 8> WorkList = collectBasicCandidates(M);

  // Propagate candidates.
  while (!WorkList.empty()) {
    Value *V = WorkList.pop_back_val();
    MutateValSet.insert(V);

    for (User *U : V->users()) {
      // collect in a user.
      SmallVector<Value *, 2> newCandidates;
      // Should only used by ld/st/sel/phi/gep/call.
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        newCandidates.emplace_back(LI);
      } else if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
        Value *Ptr = SI->getPointerOperand();
        Value *Val = SI->getValueOperand();
        if (V == Ptr)
          newCandidates.emplace_back(Val);
        else
          newCandidates.emplace_back(Ptr);
      } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
        // If result type of GEP not related to resource type, skip.
        Type *Ty = GEP->getType();
        Type *MTy = TM.mutateType(Ty);
        if (MTy == Ty) {
          // Don't recurse, but still need to mutate GEP.
          MutateValSet.insert(GEP);
          continue;
        }
        newCandidates.emplace_back(GEP);
      } else if (PHINode *Phi = dyn_cast<PHINode>(U)) {
        // Propagate all operands.
        newCandidates.emplace_back(Phi);
        for (Use &PhiOp : Phi->incoming_values()) {
          if (V == PhiOp)
            continue;
          newCandidates.emplace_back(PhiOp);
        }
      } else if (SelectInst *Sel = dyn_cast<SelectInst>(U)) {
        // Propagate other result.
        newCandidates.emplace_back(Sel);
        Value *TrueV = Sel->getTrueValue();
        Value *FalseV = Sel->getFalseValue();
        if (TrueV == V)
          newCandidates.emplace_back(FalseV);
        else
          newCandidates.emplace_back(TrueV);
      } else if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(U)) {
        if (!TM.needMutate(BCO->getType()))
          continue;
        // Make sure only used for lifetime intrinsic.
        for (User *BCUser : BCO->users()) {
          if (ConstantArray *CA = dyn_cast<ConstantArray>(BCUser)) {
            // For llvm.used.
            if (CA->hasOneUse()) {
              Value *CAUser = CA->user_back();
              if (GlobalVariable *GV = dyn_cast<GlobalVariable>(CAUser)) {
                if (GV->getName() == "llvm.used")
                  continue;
              }
            } else if (CA->user_empty()) {
              continue;
            }
          }
          CallInst *CI = cast<CallInst>(BCUser);
          Function *F = CI->getCalledFunction();
          Intrinsic::ID ID = F->getIntrinsicID();
          if (ID != Intrinsic::lifetime_start &&
              ID != Intrinsic::lifetime_end) {
            DXASSERT(false, "unexpected user");
          }
        }
      } else if (ReturnInst *RI = dyn_cast<ReturnInst>(U)) {
        newCandidates.emplace_back(RI);
      } else {
        CallInst *CI = cast<CallInst>(U);
        Type *Ty = CI->getType();
        Type *MTy = TM.mutateType(Ty);
        if (Ty != MTy)
          newCandidates.emplace_back(CI);

        SmallVector<Value *, 4> Args(CI->arg_operands().begin(),
                                     CI->arg_operands().end());
        for (Value *Arg : Args) {
          if (Arg == V)
            continue;
          Type *Ty = Arg->getType();
          Type *MTy = TM.mutateType(Ty);
          if (Ty == MTy)
            continue;
          newCandidates.emplace_back(Arg);
        }
      }

      for (Value *Val : newCandidates) {
        // New candidate find.
        if (MutateValSet.insert(Val).second) {
          WorkList.emplace_back(Val);
        }
      }
    }
  }
}

void TypeUserMutator::mutateCandidates() {
  SmallVector<Function *, 2> CandidateFns;
  for (Value *V : MutateValSet) {
    if (Function *F = dyn_cast<Function>(V)) {
      CandidateFns.emplace_back(F);
      continue;
    }
    Type *Ty = V->getType();
    Type *MTy = TM.mutateType(Ty);
    if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
      AI->setAllocatedType(MTy->getPointerElementType());
    } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
      Type *MResultEltTy = TM.mutateType(GEP->getResultElementType());
      GEP->setResultElementType(MResultEltTy);
      Type *MSrcEltTy = TM.mutateType(GEP->getSourceElementType());
      GEP->setSourceElementType(MSrcEltTy);
    } else if (GEPOperator *GEPO = dyn_cast<GEPOperator>(V)) {
      // GEP operator not support setSourceElementType.
      // Create a new GEP here.
      Constant *C = cast<Constant>(GEPO->getPointerOperand());
      IRBuilder<> B(C->getContext());
      // Make sure C is mutated so the GEP get correct sourceElementType.
      C->mutateType(TM.mutateType(C->getType()));

      // Collect user of GEPs, then replace all use with undef.
      SmallVector<Use *, 2> Uses;
      for (Use &U : GEPO->uses()) {
        Uses.emplace_back(&U);
      }

      SmallVector<Value *, 2> idxList(GEPO->idx_begin(), GEPO->idx_end());
      Type *Ty = GEPO->getType();
      GEPO->replaceAllUsesWith(UndefValue::get(Ty));
      StringRef Name = GEPO->getName();

      // GO and newGO will be same constant except has different
      // sourceElementType. ConstantMap think they're the same constant. Have to
      // remove GO first before create newGO.
      C->removeDeadConstantUsers();

      Value *newGO = B.CreateGEP(C, idxList, Name);
      // update uses.
      for (Use *U : Uses) {
        U->set(newGO);
      }
      continue;
    }
    V->mutateType(MTy);
  }

  // Mutate functions.
  for (Function *F : CandidateFns) {
    Function *MF = nullptr;

    //if (hlsl::GetHLOpcodeGroup(F) == HLOpcodeGroup::HLCast) {
    //  // Eliminate pass-through cast
    //  for (auto it = F->user_begin(); it != F->user_end();) {
    //    CallInst *CI = cast<CallInst>(*(it++));
    //    CI->replaceAllUsesWith(CI->getArgOperand(1));
    //    CI->eraseFromParent();
    //  }
    //  continue;
    //}

    if (!MF) {
      FunctionType *FT = F->getFunctionType();
      FunctionType *MFT = cast<FunctionType>(TM.mutateType(FT));

      MF = Function::Create(MFT, F->getLinkage(), "", &M);
      MF->takeName(F);

      // Copy calling conv.
      MF->setCallingConv(F->getCallingConv());
      // Copy attributes.
      AttributeSet AS = F->getAttributes();
      MF->setAttributes(AS);
      // Annotation.
      if (DxilFunctionAnnotation *FnAnnot =
              TM.getTypeSys().GetFunctionAnnotation(F)) {
        DxilFunctionAnnotation *newFnAnnot =
            TM.getTypeSys().AddFunctionAnnotation(MF);
        DxilParameterAnnotation &RetAnnot = newFnAnnot->GetRetTypeAnnotation();
        RetAnnot = FnAnnot->GetRetTypeAnnotation();
        for (unsigned i = 0; i < FnAnnot->GetNumParameters(); i++) {
          newFnAnnot->GetParameterAnnotation(i) =
              FnAnnot->GetParameterAnnotation(i);
        }
        TM.getTypeSys().FinishFunctionAnnotation(*newFnAnnot);
      }
      // Update function debug info.
      if (DISubprogram *funcDI = getDISubprogram(F))
        funcDI->replaceFunction(MF);
    }

    for (auto it = F->user_begin(); it != F->user_end();) {
      CallInst *CI = cast<CallInst>(*(it++));
      CI->setCalledFunction(MF);
    }

    if (F->isDeclaration()) {
      F->eraseFromParent();
      continue;
    }
    // Take body of F.
    // Splice the body of the old function right into the new function.
    MF->getBasicBlockList().splice(MF->begin(), F->getBasicBlockList());
    // Replace use of arg.
    auto argIt = F->arg_begin();
    for (auto MArgIt = MF->arg_begin(); MArgIt != MF->arg_end();) {
      Argument *Arg = (argIt++);
      Argument *MArg = (MArgIt++);
      Arg->replaceAllUsesWith(MArg);
      MArg->setName(Arg->getName());
    }
  }
}

bool TypeUserMutator::run() {
  collectCandidates();
  mutateCandidates();
  return !MutateValSet.empty();
}

} // namespace

namespace hlsl {
namespace hlutil {
bool mutateType(llvm::Module &M, MutateTypeFunction MutateTyFn) {
  DxilTypeSystem *pTypeSys = nullptr;
  if (M.HasHLModule()) {
    auto &HLM = M.GetHLModule();
    // FIXME: check correct version.
    //if (!HLM.GetShaderModel()->IsSM66Plus())
    //  return false;

    pTypeSys = &HLM.GetTypeSystem();
  } else if (M.HasDxilModule()) {
    auto &DM = M.GetDxilModule();
    //if (!DM.GetShaderModel()->IsSM66Plus())
    //  return false;

    pTypeSys = &DM.GetTypeSystem();
  } else {
    return false;
  }

  TypeMutator TM(MutateTyFn, *pTypeSys);
  TypeUserMutator Mutator(TM, M);
  return Mutator.run();
}
} // namespace hlutil
} // namespace hlsl
