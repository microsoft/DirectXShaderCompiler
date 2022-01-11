///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RemoveRedundantUAVCopy.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Remove redundant UAV copy caused by partial update or copy-in copy-out.   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"

#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HlslIntrinsicOp.h"


using namespace llvm;
using namespace hlsl;

///////////////////////////////////////////////////////////////////////////////
// RemoveRedundantUAVCopy.
// Remove uav store when the value is load from same address.
// Do this transform before dxil generation while address for uav is still GEP.
// After dxil generation, all the GEP will lowered to address calculation.
//
namespace {

struct LoadStorePair {
  Instruction *Load;
  Instruction *Store;
  Value *Ptr;
};

class RemoveRedundantUAVCopy : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit RemoveRedundantUAVCopy() : ModulePass(ID) {
  }

  const char *getPassName() const override {
    return "Preprocess HLModule after inline";
  }

  bool runOnModule(Module &M) override;

private:
  // Map from function to the resource the function used.
  DenseMap<Function *, DenseSet<Value *>> resUseMap;
  SmallVector<LoadInst *, 4> uavLoadCandidates;
};

char RemoveRedundantUAVCopy::ID = 0;

bool isRWStructuredBuffer(Value *Hdl) {
  CallInst *CI = dyn_cast<CallInst>(Hdl);
  if (!CI)
    return false;
  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
  if (group != HLOpcodeGroup::HLAnnotateHandle)
    return false;
  Value *props = CI->getArgOperand(HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx);
  Constant *cProps = dyn_cast<Constant>(props);
  if (!cProps)
    return false;
  auto resProps = resource_helper::loadPropsFromConstant(*cProps);
  return resProps.isUAV() &&
         resProps.getResourceKind() == DXIL::ResourceKind::StructuredBuffer;
}

struct ResSubscript {
  CallInst *CI;
  Value *Hdl;
  Value *Idx;
};

struct ResMemLocation {
  // base address of the location.
  ResSubscript Sub;
  Value *Ptr;
  // offset of the location.
  SmallVector<Value *, 4> offsets;
};

// A uav ptr include handle, subscript and gep.
// For handle,
//   global resource handle not alias dynamic resource handle.
//   global resource handle not alias other global resource handle.
//   dynamic resource handle not alias other dynamic resource handle if index is
//   different imm.
// For subscript,
//   If type is different, then not alias. Type part is done by subscript
//   function. If index is different imm, then not alias.
// For gep,
//   If offset is different imm, then not alias.

Value *skipAnnotateHandle(Value *hdl) {
  CallInst *CI = dyn_cast<CallInst>(hdl);
  if (!CI)
    return hdl;
  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
  if (group == HLOpcodeGroup::HLAnnotateHandle) {
    return CI->getArgOperand(HLOperandIndex::kAnnotateHandleHandleOpIdx);
  }
  return CI;
}

bool isCreateHandleFromHeap(CallInst *CI, HLOpcodeGroup group) {
  bool result = group == HLOpcodeGroup::HLIntrinsic;
  if (result) {

    unsigned opcode = GetHLOpcode(CI);
    IntrinsicOp subOp = static_cast<IntrinsicOp>(opcode);
    result = subOp == IntrinsicOp::IOP_CreateResourceFromHeap;
  }
  return result;
}

Value *getPtrAndOffset(Value *P, SmallVector<Value *, 4> &offsets) {
  if (GEPOperator *GEP = dyn_cast<GEPOperator>(P)) {
    offsets.append(GEP->idx_begin(), GEP->idx_end());
    return GEP->getPointerOperand();
  } else {
    return P;
  }
}
// Only different imm idx not alias.
bool idxNotAlias(Value *IdxA, Value *IdxB) {
  if (IdxA == IdxB)
    return false;

  if (isa<ConstantInt>(IdxA) && isa<ConstantInt>(IdxB)) {
    // Different imm index will not alias.
    return true;
  } else {
    return false;
  }
}

// For matrix, there will be lot of extract element and insert element like
/*
*   %7 = call <16 x float> @"dx.hl.matldst.rowLoad.<16 x float> ...
  %8 = extractelement <16 x float> %7, i64 0
  %9 = extractelement <16 x float> %7, i64 1
  %10 = extractelement <16 x float> %7, i64 2
  %11 = extractelement <16 x float> %7, i64 3
  %12 = extractelement <16 x float> %7, i64 4
  %13 = extractelement <16 x float> %7, i64 5
  %14 = extractelement <16 x float> %7, i64 6
  %15 = extractelement <16 x float> %7, i64 7
  %16 = extractelement <16 x float> %7, i64 8
  %17 = extractelement <16 x float> %7, i64 9
  %18 = extractelement <16 x float> %7, i64 10
  %19 = extractelement <16 x float> %7, i64 11
  %20 = extractelement <16 x float> %7, i64 12
  %21 = extractelement <16 x float> %7, i64 13
  %22 = extractelement <16 x float> %7, i64 14
  %23 = extractelement <16 x float> %7, i64 15

  %189 = insertelement <16 x float> undef, float %8, i64 0
  %190 = insertelement <16 x float> %189, float %9, i64 1
  %191 = insertelement <16 x float> %190, float %10, i64 2
  %192 = insertelement <16 x float> %191, float %11, i64 3
  %193 = insertelement <16 x float> %192, float %12, i64 4
  %194 = insertelement <16 x float> %193, float %13, i64 5
  %195 = insertelement <16 x float> %194, float %14, i64 6
  %196 = insertelement <16 x float> %195, float %15, i64 7
  %197 = insertelement <16 x float> %196, float %16, i64 8
  %198 = insertelement <16 x float> %197, float %17, i64 9
  %199 = insertelement <16 x float> %198, float %18, i64 10
  %200 = insertelement <16 x float> %199, float %19, i64 11
  %201 = insertelement <16 x float> %200, float %20, i64 12
  %202 = insertelement <16 x float> %201, float %21, i64 13
  %203 = insertelement <16 x float> %202, float %22, i64 14
  %204 = insertelement <16 x float> %203, float %23, i64 15
*/
bool matrixLoadStoreMatch(Value *LdMat, Value *StMat) {
  if (LdMat->getType() != StMat->getType())
    return false;
  FixedVectorType *Ty = dyn_cast<FixedVectorType>(LdMat->getType());
  if (!Ty)
    return false;
  unsigned size = Ty->getNumElements();
  if (!LdMat->hasNUses(size))
    return false;

  // Make sure LdMat only used by extract element, and each element only extract once.
  const unsigned kMaxMatrixNumElements = 16;
  std::vector<InsertElementInst *> Elements(kMaxMatrixNumElements);

  for (User *U : LdMat->users()) {
    ExtractElementInst *EEI = dyn_cast<ExtractElementInst>(U);
    if (!EEI)
      return false;
    if (!EEI->hasOneUse())
      return false;
    InsertElementInst *IEI = dyn_cast<InsertElementInst>(EEI->user_back());
    if (!IEI)
      return false;
    Value *Idx = EEI->getIndexOperand();
    if (IEI->getOperand(2) != Idx)
      return false;
    ConstantInt *CIdx = dyn_cast<ConstantInt>(Idx);
    if (!CIdx)
      return false;
    unsigned immIdx = CIdx->getLimitedValue();
    if (immIdx >= size)
      return false;
    Elements[immIdx] = IEI;
  }
  // The last InsertElements must be StMat.
  if (StMat != Elements[size - 1])
    return false;
  // First vector must be undef.
  if (!isa<UndefValue>(Elements[0]->getOperand(0)))
    return false;
  // Make sure InsertElements is by order.
  for (unsigned i = 1; i < size; ++i) {
    InsertElementInst *CurElt = Elements[i];
    Value *PrevVec = CurElt->getOperand(0);
    if (PrevVec != Elements[i - 1])
      return false;
  }
  return true;
}


bool offsetsNotAlias(SmallVector<Value *, 4> &offsetA,
                     SmallVector<Value *, 4> &offsetB) {
  // offset size mismatch not alias.
  if (offsetA.size() != offsetB.size())
    return true;
  bool bHasNotAliasIdx = false;
  // One idx not alias will cause the ptr not alias.
  for (unsigned i = 0; i < offsetA.size(); ++i) {
    Value *idxA = offsetA[i];
    Value *idxB = offsetB[i];
    if (idxNotAlias(idxA, idxB)) {
      bHasNotAliasIdx = true;
      break;
    }
  }
  return bHasNotAliasIdx;
}

bool handleNotAlias(Value *a, Value *b) {
  Value *hdlA = skipAnnotateHandle(a);
  Value *hdlB = skipAnnotateHandle(b);
  if (hdlA == hdlB)
    return false;
  CallInst *CIA = dyn_cast<CallInst>(hdlA);
  CallInst *CIB = dyn_cast<CallInst>(hdlB);
  if (!CIA || !CIB)
    return false;

  HLOpcodeGroup groupA = GetHLOpcodeGroupByName(CIA->getCalledFunction());
  HLOpcodeGroup groupB = GetHLOpcodeGroupByName(CIB->getCalledFunction());
  bool isUserFnA = groupA == HLOpcodeGroup::NotHL;
  bool isUserFnB = groupB == HLOpcodeGroup::NotHL;
  // Skip user function.
  if (isUserFnA || isUserFnB)
    return false;

  bool isCreateHandleA = groupA == HLOpcodeGroup::HLCreateHandle;
  bool isCreateHandleB = groupB == HLOpcodeGroup::HLCreateHandle;
  if (isCreateHandleA && isCreateHandleB) {
    // When both created from global resource, check if from same resource.
    Value *resA =
        CIA->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    Value *resB =
        CIB->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    if (resA == resB)
      return false;

    LoadInst *ldA = dyn_cast<LoadInst>(resA);
    LoadInst *ldB = dyn_cast<LoadInst>(resB);
    SmallVector<Value *, 4> offsetA;
    SmallVector<Value *, 4> offsetB;
    Value *ptrA = getPtrAndOffset(ldA->getPointerOperand(), offsetA);
    Value *ptrB = getPtrAndOffset(ldB->getPointerOperand(), offsetB);

    GlobalVariable *GVA = dyn_cast<GlobalVariable>(ptrA);
    GlobalVariable *GVB = dyn_cast<GlobalVariable>(ptrB);
    // Skip local resource.
    if (!GVA || !GVB)
      return false;
    // Skip static global resource.
    if (GVA->getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage ||
        GVB->getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage)
      return false;

    // Different global resource not alias.
    if (GVA != GVB)
      return true;

    return offsetsNotAlias(offsetA, offsetB);
  }

  bool isCreateHandleFromHeapA = isCreateHandleFromHeap(CIA, groupA);
  bool isCreateHandleFromHeapB = isCreateHandleFromHeap(CIB, groupB);
  if (isCreateHandleFromHeapA && isCreateHandleFromHeapB) {
    // When both dynamic resource, check the index.
    Value *IdxA =
        CIA->getArgOperand(HLOperandIndex::kCreateHandleFromHeapIndexOpIdx);
    Value *IdxB =
        CIB->getArgOperand(HLOperandIndex::kCreateHandleFromHeapIndexOpIdx);

    return idxNotAlias(IdxA, IdxB);
  }

  // global resource not alias with dynamic resource.
  if ((isCreateHandleA && isCreateHandleFromHeapB) ||
      (isCreateHandleB && isCreateHandleFromHeapA))
    return true;

  return false;
}

bool notAlias(ResSubscript &a, ResSubscript &b) {
  if (idxNotAlias(a.Idx, b.Idx))
    return true;

  if (handleNotAlias(a.Hdl, b.Hdl))
    return true;

  return false;
}

bool notAlias(ResMemLocation &a, ResMemLocation &b) {
  if (notAlias(a.Sub, b.Sub))
    return true;
  if (offsetsNotAlias(a.offsets, b.offsets))
    return true;
  return false;
}

bool isSameOffset(SmallVector<Value *, 4> &offsetA,
                  SmallVector<Value *, 4> &offsetB) {
  if (offsetA.size() != offsetB.size())
    return false;
  for (unsigned i = 0; i < offsetA.size(); ++i) {
    if (offsetA[i] != offsetB[i])
      return false;
  }

  return true;
}

bool isSameHandle(Value *a, Value *b) {
  if (a == b)
    return true;
  CallInst *CIA = dyn_cast<CallInst>(a);
  CallInst *CIB = dyn_cast<CallInst>(b);
  if (!CIA || !CIB)
    return false;

  HLOpcodeGroup groupA = GetHLOpcodeGroupByName(CIA->getCalledFunction());
  HLOpcodeGroup groupB = GetHLOpcodeGroupByName(CIB->getCalledFunction());
  if (groupA != groupB)
    return false;
  if (groupA == HLOpcodeGroup::HLAnnotateHandle) {
    Value *propsA = CIA->getArgOperand(
        HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx);
    Value *propsB = CIB->getArgOperand(
        HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx);
    if (propsA != propsB)
      return false;
    a = CIA->getArgOperand(HLOperandIndex::kAnnotateHandleHandleOpIdx);
    b = CIB->getArgOperand(HLOperandIndex::kAnnotateHandleHandleOpIdx);
  }
  if (a == b)
    return true;

  CIA = dyn_cast<CallInst>(a);
  CIB = dyn_cast<CallInst>(b);
  if (!CIA || !CIB)
    return false;

  groupA = GetHLOpcodeGroupByName(CIA->getCalledFunction());
  groupB = GetHLOpcodeGroupByName(CIB->getCalledFunction());
  if (groupA != groupB)
    return false;

  if (groupA == HLOpcodeGroup::HLCreateHandle) {
    // When both created from global resource, check if from same resource.
    Value *resA =
        CIA->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    Value *resB =
        CIB->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    if (resA == resB)
      return true;

    LoadInst *ldA = dyn_cast<LoadInst>(resA);
    LoadInst *ldB = dyn_cast<LoadInst>(resB);
    SmallVector<Value *, 4> offsetA;
    SmallVector<Value *, 4> offsetB;
    Value *ptrA = getPtrAndOffset(ldA->getPointerOperand(), offsetA);
    Value *ptrB = getPtrAndOffset(ldB->getPointerOperand(), offsetB);
    if (ptrA != ptrB)
      return false;

    return isSameOffset(offsetA, offsetB);
  } else if (isCreateHandleFromHeap(CIA, groupA)) {
    // When both dynamic resource, check the index.
    Value *IdxA =
        CIA->getArgOperand(HLOperandIndex::kCreateHandleFromHeapIndexOpIdx);
    Value *IdxB =
        CIB->getArgOperand(HLOperandIndex::kCreateHandleFromHeapIndexOpIdx);

    return IdxA == IdxB;
  } else {
    return false;
  }
}

bool isSameResSubscript(ResSubscript &a, ResSubscript &b) {
  if (a.Idx != b.Idx)
    return false;
  return isSameHandle(a.Hdl, b.Hdl);
}

bool isSameMemLoc(ResMemLocation &a, ResMemLocation &b) {
  if (!isSameResSubscript(a.Sub, b.Sub))
    return false;
  return isSameOffset(a.offsets, b.offsets);
}

bool isLoadStorePair(Value *a, Value *b,
                     SmallVector<Instruction *, 8> &CandidateLoads,
                     SmallVector<Instruction *, 8> &CandidateStores) {
  if (StoreInst *SI = dyn_cast<StoreInst>(a)) {
    LoadInst *LI = dyn_cast<LoadInst>(b);
    if (!LI)
      return false;
    if (SI->getValueOperand() != LI)
      return false;
    CandidateStores.emplace_back(SI);
    CandidateLoads.emplace_back(LI);
    return true;
  } else if (LoadInst *LI = dyn_cast<LoadInst>(a)) {
    StoreInst *SI = dyn_cast<StoreInst>(b);
    if (!SI)
      return false;
    if (SI->getValueOperand() != LI)
      return false;
    CandidateStores.emplace_back(SI);
    CandidateLoads.emplace_back(LI);
    return true;
  } else if (CallInst *CI = dyn_cast<CallInst>(a)) {
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    if (group != HLOpcodeGroup::HLMatLoadStore)
      return false;
    CallInst *CIB = dyn_cast<CallInst>(b);
    if (!CIB)
      return false;
    group = GetHLOpcodeGroupByName(CIB->getCalledFunction());
    if (group != HLOpcodeGroup::HLMatLoadStore)
      return false;
    unsigned opcode = GetHLOpcode(CI);
    HLMatLoadStoreOpcode subOpA = static_cast<HLMatLoadStoreOpcode>(opcode);
    opcode = GetHLOpcode(CIB);
    HLMatLoadStoreOpcode subOpB = static_cast<HLMatLoadStoreOpcode>(opcode);
    switch (subOpA) {
    case HLMatLoadStoreOpcode::RowMatLoad:
      if (subOpB != HLMatLoadStoreOpcode::RowMatStore)
        return false;
      if (!matrixLoadStoreMatch(
              CI, CIB->getArgOperand(HLOperandIndex::kMatStoreValOpIdx)))
        return false;

      CandidateStores.emplace_back(CIB);
      CandidateLoads.emplace_back(CI);
      return true;
    case HLMatLoadStoreOpcode::ColMatLoad:
      if (subOpB != HLMatLoadStoreOpcode::ColMatStore)
        return false;
      if (!matrixLoadStoreMatch(
              CI, CIB->getArgOperand(HLOperandIndex::kMatStoreValOpIdx)))
        return false;
      CandidateStores.emplace_back(CIB);
      CandidateLoads.emplace_back(CI);
      return true;
    case HLMatLoadStoreOpcode::ColMatStore:
      if (subOpB != HLMatLoadStoreOpcode::ColMatLoad)
        return false;
      if (!matrixLoadStoreMatch(
              CIB, CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx)))
        return false;
      CandidateStores.emplace_back(CI);
      CandidateLoads.emplace_back(CIB);
      return true;
    case HLMatLoadStoreOpcode::RowMatStore:
      if (subOpB != HLMatLoadStoreOpcode::RowMatLoad)
        return false;
      if (!matrixLoadStoreMatch(
              CIB, CI->getArgOperand(HLOperandIndex::kMatStoreValOpIdx)))
        return false;
      CandidateStores.emplace_back(CI);
      CandidateLoads.emplace_back(CIB);
      return true;
    }
  } else {
    return false;
  }
  return false;
}

} // namespace

bool RemoveRedundantUAVCopy::runOnModule(Module &M) {
  bool bUpdated = false;

  for (Function &F : M.functions()) {
    if (!F.isDeclaration())
      continue;

    // skip if not subscript.
    HLOpcodeGroup group = GetHLOpcodeGroupByName(&F);
    if (group != HLOpcodeGroup::HLSubscript)
      continue;

    SmallVector<ResSubscript, 4> ResSubscripts;

    bool bSubscriptUAV = true;
    for (User *U : F.users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      unsigned opcode = GetHLOpcode(CI);
      HLSubscriptOpcode subOp = static_cast<HLSubscriptOpcode>(opcode);
      if (subOp != HLSubscriptOpcode::DefaultSubscript) {
        bSubscriptUAV = false;
        break;
      }
      Value *Hdl = CI->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);
      if (!isRWStructuredBuffer(Hdl)) {
        bSubscriptUAV = false;
        break;
      }
      Value *Idx = CI->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx);
      // Merge gep to make collect use easier.
      HLModule::MergeGepUse(CI);
      ResSubscript Sub = {CI, Hdl, Idx};
      ResSubscripts.emplace_back(Sub);
    }

    if (!bSubscriptUAV)
      continue;
    if (ResSubscripts.empty())
      continue;

    SmallVector<ResMemLocation, 16> Locs;
    SmallVector<SmallVector<Value *, 2>, 4> LocUsers;
    // Collect all use.
    for (auto &Sub : ResSubscripts) {
      for (User *U : Sub.CI->users()) {
        // Create Mem Location.
        ResMemLocation Loc;
        Loc.Sub = Sub;
        Loc.Ptr = U;
        getPtrAndOffset(U, Loc.offsets);

        // Check if same offset match.
        bool bIsDup = false;
        for (unsigned i = 0; i < Locs.size(); ++i) {
          auto &CurLoc = Locs[i];
          if (!isSameMemLoc(Loc, CurLoc))
            continue;
          LocUsers[i].append(U->user_begin(), U->user_end());
          bIsDup = true;
          break;
        }
        if (!bIsDup) {
          Locs.emplace_back(Loc);
          LocUsers.emplace_back();
          LocUsers.back().append(U->user_begin(), U->user_end());
        }
      }
    }

    SmallVector<unsigned, 8> Candidates;
    SmallVector<Instruction *, 8> CandidateLoads;
    SmallVector<Instruction *, 8> CandidateStores;
    // Iterate all location, find load store pairs.
    for (unsigned i = 0; i < LocUsers.size(); ++i) {
      if (LocUsers[i].size() != 2)
        continue;
      Value *a = LocUsers[i][0];
      Value *b = LocUsers[i][1];
      if (!isLoadStorePair(a, b, CandidateLoads, CandidateStores))
        continue;
      Candidates.emplace_back(i);
    }

    // Iterate Candidates.
    // If no alias in Locs, then it is OK to remove.
    for (auto idx : Candidates) {
      ResMemLocation &Loc = Locs[idx];
      bool bSafeToRemove = true;
      // TODO:Things in different entry will not alias.
      for (unsigned i = 0; i < Locs.size(); ++i) {
        if (i == idx)
          continue;
        if (notAlias(Loc, Locs[i]))
          continue;
        bSafeToRemove = false;
        break;
      }
      if (!bSafeToRemove)
        continue;
      bUpdated = true;
      // Erase store first.
      CandidateStores[idx]->eraseFromParent();
      Instruction *Ld = CandidateLoads[idx];
      // For case the ld is used, just leave it.
      if (Ld->user_empty()) {
        Ld->eraseFromParent();
      }
    }
  }

  return bUpdated;
}

ModulePass *llvm::createRemoveRedundantUAVCopyPass() {
  return new RemoveRedundantUAVCopy();
}

INITIALIZE_PASS(RemoveRedundantUAVCopy, "hlsl-remove-reduandant-uav-copy",
                "Remove redundant uav copy", false, false)