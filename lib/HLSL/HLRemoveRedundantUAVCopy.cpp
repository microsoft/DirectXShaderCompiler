///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLRemoveRedundantUAVCopy.cpp                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Remove redundant UAV copy caused by copy-in copy-out.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLUtil.h"
#include "dxc/HlslIntrinsicOp.h"

using namespace llvm;
using namespace hlsl;

#define DEBUG_TYPE "hl-remove-redundant-uav-ldst"

namespace {
// Not a real reach info.
// It not consider loop case.
// Only for reach check when 2 blocks are in same loop.
class SameLoopReachInfo {
public:
  SameLoopReachInfo() {}
  SmallPtrSet<BasicBlock *, 32> &getReachSet(BasicBlock *BB) {
    return ReachMap[BB];
  }
  bool canReach(BasicBlock *Start, BasicBlock *Target) {
    return ReachMap[Start].count(Target) > 0;
  }
  void build(Function &F) {
    for (auto BB : post_order(&F.getEntryBlock())) {
      auto &CurReach = ReachMap[BB];
      for (BasicBlock *Pred : predecessors(BB)) {
        auto &PredReach = ReachMap[Pred];
        // Pred can reach BB.
        PredReach.insert(BB);
        // Pred can reach all things BB can reach.
        PredReach.insert(CurReach.begin(), CurReach.end());
        // BB can reach Pred.
        // A loop.
        if (CurReach.count(Pred) != 0) {
          // Don't update ReachMap for loop.
          // Only support case when 2 blocks are in same loop.
        }
      }
    }
  }

private:
  DenseMap<BasicBlock *, SmallPtrSet<BasicBlock *, 32>> ReachMap;
};
} // namespace

namespace {

bool isSamePtr(Value *Ptr0, Value *Ptr1) {
  if (Ptr0 == Ptr1)
    return true;
  // FIXME: support GEP.
  return false;
}

bool isSameIndex(Value *Idx0, Value *Idx1, DominatorTree &DT) {
  // For case imm index.
  if (Idx0 == Idx1)
    return true;

  // Only support LoadInst.
  LoadInst *LI0 = dyn_cast<LoadInst>(Idx0);
  if (!LI0)
    return false;
  LoadInst *LI1 = dyn_cast<LoadInst>(Idx1);
  if (!LI1)
    return false;

  Value *Ptr0 = LI0->getPointerOperand();
  Value *Ptr1 = LI1->getPointerOperand();
  if (!isSamePtr(Ptr0, Ptr1))
    return false;

  // Make sure no write in between 2 loads.
  // To make things easy, just check the Ptr only has 1 store.
  hlutil::PointerStatus PS(Ptr0, /*size*/ 0, /*bLdStOnly*/ false);
  const bool bStructElt = false;
  // Not care function annotation, just make an empty typeSys here.
  DxilTypeSystem typeSys(nullptr);
  PS.analyze(typeSys, bStructElt);
  switch (PS.storedType) {
  case hlutil::PointerStatus::StoredType::Stored:
    return false;
  case hlutil::PointerStatus::StoredType::MemcopyDestOnce:
    // Ptr has memcpy should not have load inst.
    return false;
  case hlutil::PointerStatus::StoredType::StoredOnce: {
    return DT.dominates(PS.StoredOnceInst, LI0) &&
           DT.dominates(PS.StoredOnceInst, LI1);
  }
  case hlutil::PointerStatus::StoredType::NotStored:
    return isa<GlobalVariable>(Ptr0) &&
           cast<GlobalVariable>(Ptr0)->getLinkage() ==
               GlobalValue::LinkageTypes::ExternalLinkage;
  case hlutil::PointerStatus::StoredType::InitializerStored:
    return isa<GlobalVariable>(Ptr0) &&
           cast<GlobalVariable>(Ptr0)->getLinkage() ==
               GlobalValue::LinkageTypes::InternalLinkage;
  }
  return false;
}

bool isSameHandle(Value *Hdl0, Value *Hdl1, DominatorTree &DT) {
  if (Hdl0 == Hdl1)
    return true;

  CallInst *AnnotHdl0 = dyn_cast<CallInst>(Hdl0);
  if (!AnnotHdl0)
    return false;
  CallInst *AnnotHdl1 = dyn_cast<CallInst>(Hdl1);
  if (!AnnotHdl1)
    return false;
  // Check annotate handle.
  if (AnnotHdl0->getArgOperand(
          HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx) !=
      AnnotHdl1->getArgOperand(
          HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx))
    return false;

  HLOpcodeGroup group = GetHLOpcodeGroupByName(AnnotHdl0->getCalledFunction());
  if (group != HLOpcodeGroup::HLAnnotateHandle)
    return false;

  unsigned opcode0 = GetHLOpcode(AnnotHdl0);
  unsigned opcode1 = GetHLOpcode(AnnotHdl1);
  if (opcode0 != opcode1)
    return false;

  // Check createHandle.
  CallInst *HdlCI0 = dyn_cast<CallInst>(
      AnnotHdl0->getArgOperand(HLOperandIndex::kAnnotateHandleHandleOpIdx));
  CallInst *HdlCI1 = dyn_cast<CallInst>(
      AnnotHdl1->getArgOperand(HLOperandIndex::kAnnotateHandleHandleOpIdx));

  if (HdlCI0->getCalledFunction() != HdlCI1->getCalledFunction())
    return false;

  group = GetHLOpcodeGroupByName(HdlCI1->getCalledFunction());
  if (group != HLOpcodeGroup::HLCreateHandle)
    return false;

  Value *Res0 =
      HdlCI0->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
  Value *Res1 =
      HdlCI1->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);

  if (Res0 == Res1)
    return true;

  return isSameIndex(Res0, Res1, DT);
}

bool isSameUavSubScript(Value *Uav0, Value *Uav1, DominatorTree &DT) {
  if (Uav0 == Uav1)
    return true;

  CallInst *Sub0 = dyn_cast<CallInst>(Uav0);
  if (!Sub0)
    return false;
  CallInst *Sub1 = dyn_cast<CallInst>(Uav1);
  if (!Sub1)
    return false;

  if (Sub0->getCalledFunction() != Sub1->getCalledFunction())
    return false;

  if (!isSameHandle(Sub0->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx),
                    Sub1->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx),
                    DT))
    return false;

  if (!isSameIndex(Sub0->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx),
                   Sub1->getArgOperand(HLOperandIndex::kSubscriptIndexOpIdx),
                   DT))
    return false;

  return true;
}
} // namespace

namespace {
// Check if a handle is for RWStructuredBuffer.
bool isRWStructuredBuffer(Value *Hdl) {
  CallInst *CI = dyn_cast<CallInst>(Hdl);
  if (!CI)
    return false;
  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
  if (group != HLOpcodeGroup::HLAnnotateHandle)
    return false;
  Value *props =
      CI->getArgOperand(HLOperandIndex::kAnnotateHandleResourcePropertiesOpIdx);
  Constant *cProps = dyn_cast<Constant>(props);
  if (!cProps)
    return false;
  auto resProps = resource_helper::loadPropsFromConstant(*cProps);
  return resProps.isUAV() &&
         resProps.getResourceKind() == DXIL::ResourceKind::StructuredBuffer;
}

struct UavAccessInfo {
  SmallPtrSet<Function *, 4> FunctionAccessSet;
  DenseMap<BasicBlock *, SmallVector<Instruction *, 4>> BlockAccessMap;
  void addAccess(Instruction *I) {
    BasicBlock *BB = I->getParent();
    Function *F = BB->getParent();
    BlockAccessMap[BB].emplace_back(I);
    FunctionAccessSet.insert(F);
  }
  void append(UavAccessInfo &Info) {
    for (auto &AccessMap : Info.BlockAccessMap) {
      for (Instruction *I : AccessMap.second) {
        addAccess(I);
      }
    }
  }
};

// Pattern is
//  Tmp = Uav0; // UavLdCpy
//  Update Tmp; // partial update.
//  Uav1 = Tmp; // UavStCpy
// return true if there's no uav access between UavLdCpy and UavStCpy.
bool noAccessUAVInBetween(MemCpyInst *UavStCpy, MemCpyInst *UavLdCpy,
                          UavAccessInfo &Info, SameLoopReachInfo &ReachInfo) {
  // If UAV1 is not accessed between memcpy0 and memcpy1, then it is safe to
  // replace Tmp with UAV1.
  BasicBlock *LdBB = UavLdCpy->getParent();
  BasicBlock *StBB = UavStCpy->getParent();
  if (LdBB == StBB) {
    // Get UavAccessList for the BB.
    // Make sure nothing between UavLdCpy and UavStCpy.
    auto &AccessList = Info.BlockAccessMap[LdBB];
    for (auto it = AccessList.begin(); it != AccessList.end(); ++it) {
      Instruction *I = *it;
      if (I == UavStCpy)
        return false;
      if (I == UavLdCpy) {
        it++;
        if (it == AccessList.end())
          return false;
        if (*it == UavStCpy)
          return true;
      }
    }
    return false;
  } else {
    // Get UavAccessList for LdBB.
    // Make sure nothing after UavLdCpy.
    auto &LdAccessList = Info.BlockAccessMap[LdBB];
    if (LdAccessList.empty() || LdAccessList.back() != UavLdCpy)
      return false;
    // Get UavAccessList for StBB.
    // Make sure nothing before UavStCpy.
    auto &StAccessList = Info.BlockAccessMap[StBB];
    if (StAccessList.empty() || StAccessList.front() != UavStCpy)
      return false;
    // Check UavAccessList for all BB between LdBB and StBB.
    // A path exist LdBB -> BB -> StBB.
    // All BB which LdBB can reach and can reach StBB.
    // Make sure Uav access in BB.
    auto &ReachSet = ReachInfo.getReachSet(LdBB);
    for (BasicBlock *BB : ReachSet) {
      if (!ReachInfo.canReach(BB, StBB))
        continue;
      if (!Info.BlockAccessMap[BB].empty())
        return false;
    }
  }
  return true;
}

Type *getResReturnType(CallInst *AnnotateHandle) {
  // Check the users untill find a buf_ld or subscript.
  // If not, then return nullptr, this handle will be skipped because only ld
  // and subscript will access memory.
  for (User *U : AnnotateHandle->users()) {
    CallInst *CI = dyn_cast<CallInst>(U);
    if (!CI)
      continue;
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    if (group == HLOpcodeGroup::HLSubscript) {
      return CI->getType()->getPointerElementType();
    }
    if (group == HLOpcodeGroup::HLIntrinsic) {
      unsigned opcode = GetHLOpcode(CI);
      if (opcode == (unsigned)hlsl::IntrinsicOp::MOP_Load) {
        return CI->getType();
      }
    }
  }
  return nullptr;
}

void collectUavSubscriptAccess(Value *Ptr, UavAccessInfo &Info,
                               SmallPtrSet<Value *, 4> &Visited) {
  if (Visited.count(Ptr))
    return;
  Visited.insert(Ptr);
  for (User *U : Ptr->users()) {
    if (LoadInst *LI = dyn_cast<LoadInst>(U))
      Info.addAccess(LI);
    else if (StoreInst *SI = dyn_cast<StoreInst>(U))
      Info.addAccess(SI);
    else if (isa<GEPOperator>(U) || isa<BitCastOperator>(U) ||
             isa<CastInst>(U) || isa<PHINode>(U))
      collectUavSubscriptAccess(U, Info, Visited);
    else if (CallInst *CI = dyn_cast<CallInst>(U))
      Info.addAccess(CI);
    else
      DXASSERT(0, "unsupported UAV subscript user");
  }
}

void collectUavAccess(Value *Handle, UavAccessInfo &Info) {
  for (User *U : Handle->users()) {
    CallInst *CI = dyn_cast<CallInst>(U);
    if (!CI)
      continue;
    HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
    // For RWStructuredBuffer, only subscript and load access memory.
    if (group == HLOpcodeGroup::HLSubscript) {
      SmallPtrSet<Value *, 4> Visited;
      collectUavSubscriptAccess(CI, Info, Visited);
      continue;
    }
    if (group == HLOpcodeGroup::HLIntrinsic) {
      unsigned opcode = GetHLOpcode(CI);
      if (opcode == (unsigned)hlsl::IntrinsicOp::MOP_Load) {
        Info.addAccess(CI);
        continue;
      }
    }
  }
}

UavAccessInfo collectExternalUavAccess(Module &M) {
  // FIXME: propagate readnone attribute if needed.
  // Function called by readnone function should be readnone.

  // Add all external function calls as access.
  SmallPtrSet<Function *, 4> ExternFunctionAccess;
  // Collect functions which might read/write UAV.
  for (Function &F : M.functions()) {
    // Skip functions which not touch memeory.
    if (F.hasFnAttribute(Attribute::AttrKind::ReadNone))
      continue;
    // Skip llvm intrinsics.
    if (F.getIntrinsicID() != Intrinsic::ID::not_intrinsic)
      continue;
    HLOpcodeGroup Group = GetHLOpcodeGroupByName(&F);
    // Skip HL intrinsics. The call to UAV related intrinsic is collected in
    // collectUAVAccess per UAV.
    if (Group != HLOpcodeGroup::NotHL)
      continue;
    if (!F.isDeclaration())
      continue;
    // Assume externl function touch UAV by default.
    ExternFunctionAccess.insert(&F);
  }

  UavAccessInfo ExternUAVAccessInfo;
  for (auto *F : ExternFunctionAccess) {
    for (User *U : F->users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      // Might be bitcast for metadata.
      if (!CI)
        continue;
      Function *Caller = CI->getParent()->getParent();
      if (Caller->hasFnAttribute(Attribute::AttrKind::ReadNone))
        continue;
      ExternUAVAccessInfo.addAccess(CI);
    }
  }
  return ExternUAVAccessInfo;
}

void addExternAccessAndPropagate(UavAccessInfo &Info,
                                 UavAccessInfo &ExternUAVAccessInfo) {

  // Add ExternUAVAccessInfo.
  Info.append(ExternUAVAccessInfo);

  // Propagate function access.
  SmallPtrSet<Function *, 4> Visited;
  while (true) {
    UavAccessInfo TmpUAVAccessInfo;
    for (Function *F : Info.FunctionAccessSet) {
      if (Visited.count(F))
        continue;
      Visited.insert(F);
      for (User *U : F->users()) {
        CallInst *CI = dyn_cast<CallInst>(U);
        if (!CI)
          continue;
        TmpUAVAccessInfo.addAccess(CI);
      }
    }
    // Break when no new function add.
    if (TmpUAVAccessInfo.FunctionAccessSet.empty()) {
      break;
    }
    // Move to TmpInfo to Info.
    Info.append(TmpUAVAccessInfo);
  }

  // Sort block access list.
  for (auto it : Info.BlockAccessMap) {
    BasicBlock *BB = it.first;
    SmallPtrSet<Instruction *, 4> AccessSet;
    auto &AccessList = it.second;
    for (Instruction *I : AccessList) {
      AccessSet.insert(I);
    }
    AccessList.clear();
    // Add inst back in order.
    for (Instruction &I : *BB) {
      if (AccessSet.count(&I) == 0)
        continue;
      AccessSet.erase(&I);
      AccessList.emplace_back(&I);
    }
  }
}

SmallDenseMap<Type *, UavAccessInfo>
collectUavAccess(Module &M, const SmallPtrSet<Type *, 4> &CandidateTypeSet) {
  SmallDenseMap<Type *, UavAccessInfo> InfoMap;
  DenseMap<Type *, SmallVector<CallInst *, 4>> AnnotateHandleMap;
  // collect all RWStructuredBuffer handles from annotateHandle.
  for (Function &F : M.functions()) {
    if (!F.isDeclaration())
      continue;

    // skip if not subscript.
    HLOpcodeGroup group = GetHLOpcodeGroupByName(&F);
    if (group != HLOpcodeGroup::HLAnnotateHandle)
      continue;

    for (User *U : F.users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      if (!isRWStructuredBuffer(CI))
        continue;
      Type *ResRetTy = getResReturnType(CI);
      // Skip ResRetTy which not have candidate.
      if (CandidateTypeSet.count(ResRetTy) == 0)
        continue;
      AnnotateHandleMap[ResRetTy].emplace_back(CI);
    }
  }

  // add UavAccess.
  for (auto &it : AnnotateHandleMap) {
    UavAccessInfo &Info = InfoMap[it.first];
    for (Value *Handle : it.second)
      collectUavAccess(Handle, Info);
  }

  UavAccessInfo ExternUAVAccessInfo = collectExternalUavAccess(M);
  // add extern access and propagate.
  for (auto &it : InfoMap) {
    UavAccessInfo &Info = it.second;
    addExternAccessAndPropagate(Info, ExternUAVAccessInfo);
  }
  return InfoMap;
}

bool isWriteNoneFunction(Function *F) {
  if (!F)
    return false;

  if ((F->hasFnAttribute(Attribute::AttrKind::ReadNone) ||
       F->hasFnAttribute(Attribute::AttrKind::ReadOnly)))
    return true;
  auto ID = F->getIntrinsicID();
  if (ID == Intrinsic::ID::lifetime_start || ID == Intrinsic::ID::lifetime_end)
    return true;
  return false;
}

struct WriteSlot {
  unsigned Offset;
  unsigned Size;
};

struct AllocaWrite {
  Instruction *Inst;
  // GEP used to make the pointer.
  SmallVector<Instruction *, 4> AddressList;
  AllocaWrite(Instruction *I) : Inst(I) {}
  AllocaWrite(Instruction *I, SmallVector<Instruction *, 4> &Addrs)
      : Inst(I), AddressList(Addrs.begin(), Addrs.end()) {}
  WriteSlot cacluateSlots(const DataLayout &DL);
};

WriteSlot AllocaWrite::cacluateSlots(const DataLayout &DL) {
  unsigned Offset = 0;
  // Assume all write have 1 GEP.
  DXASSERT(AddressList.size() == 1,
           "isPartialWrite fail to confirm all write should have 1 GEP");
  GEPOperator *GEP = cast<GEPOperator>(AddressList.front());
  unsigned Size = DL.getTypeAllocSize(GEP->getType()->getPointerElementType());
  for (gep_type_iterator GTI = gep_type_begin(GEP), GTE = gep_type_end(GEP);
       GTI != GTE; ++GTI) {
    ConstantInt *OpC = dyn_cast<ConstantInt>(GTI.getOperand());
    if (!OpC) {
      unsigned Size = DL.getTypeAllocSize(GTI.getIndexedType());
      // Dynamic indexing.
      // Now treat dynamic indexing as read everything.
      // FIXME: only mark size in GEP for dynamic indexing.
      return WriteSlot{Offset, Size};
    }
    if (OpC->isZero())
      continue;

    // Handle a struct index, which adds its field offset to the pointer.
    if (StructType *STy = dyn_cast<StructType>(*GTI)) {
      unsigned ElementIdx = OpC->getZExtValue();
      const StructLayout *SL = DL.getStructLayout(STy);
      Offset += SL->getElementOffset(ElementIdx);
      continue;
    }

    unsigned TypeSize = DL.getTypeAllocSize(GTI.getIndexedType());
    Offset += OpC->getValue().getLimitedValue() * TypeSize;
  }
  return WriteSlot{Offset, Size};
}

void collectAllocaWrites(Value *Ptr, SmallVector<AllocaWrite, 4> &Writes,
                         SmallVector<Instruction *, 4> &GEPList,
                         SmallPtrSet<Value *, 4> &Visited) {
  if (Visited.count(Ptr))
    return;
  Visited.insert(Ptr);
  for (User *U : Ptr->users()) {
    if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
      continue;
    } else if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
      if (Ptr == SI->getPointerOperand())
        Writes.emplace_back(AllocaWrite{SI, GEPList});
    } else if (isa<GEPOperator>(U) || isa<BitCastOperator>(U) ||
               isa<CastInst>(U) || isa<PHINode>(U)) {
      SmallVector<Instruction *, 4> CurGEPList = GEPList;
      CurGEPList.emplace_back(cast<Instruction>(U));
      collectAllocaWrites(U, Writes, CurGEPList, Visited);
    } else if (CallInst *CI = dyn_cast<CallInst>(U)) {
      Function *F = CI->getCalledFunction();
      if (isWriteNoneFunction(F)) {
        HLOpcodeGroup Group = GetHLOpcodeGroupByName(F);
        switch (Group) {
        default:
          break;
        case HLOpcodeGroup::HLSubscript:
          SmallVector<Instruction *, 4> CurGEPList = GEPList;
          CurGEPList.emplace_back(CI);
          collectAllocaWrites(U, Writes, CurGEPList, Visited);
          break;
        }
        continue;
      }
      Writes.emplace_back(AllocaWrite(CI, GEPList));
    } else
      DXASSERT(0, "unsupported alloca user");
  }
}

bool allAllocaWritesInBetween(Value *Ptr, MemCpyInst *Begin, MemCpyInst *End,
                              DominatorTree &DT, SameLoopReachInfo &RI,
                              SmallVector<AllocaWrite, 4> &AllocWrites) {
  BasicBlock *EndBB = End->getParent();
  for (auto &Write : AllocWrites) {
    Instruction *I = Write.Inst;
    if (I == Begin)
      continue;
    if (I == End)
      continue;
    if (!DT.dominates(Begin, I))
      return false;
    BasicBlock *BB = I->getParent();
    if (BB == EndBB) {
      if (!DT.dominates(I, End))
        return false;
    } else if (!RI.canReach(BB, EndBB)) {
      return false;
    }
    // Collect address of the write.
  }

  return true;
}

bool isPartialWrite(Value *Ptr, Module &M, MemCpyInst *Begin, MemCpyInst *End,
                    SmallVector<AllocaWrite, 4> &AllocWrites) {
  // If only partial of Ptr is written.
  const DataLayout &DL = M.getDataLayout();
  Type *Ty = Ptr->getType()->getPointerElementType();
  unsigned Size = DL.getTypeAllocSize(Ty);

  SmallVector<WriteSlot, 4> Slots;
  for (auto &Write : AllocWrites) {
    if (Write.Inst == Begin)
      continue;
    if (Write.Inst == End)
      continue;
    // Only support 1 GEP.
    // If not have GEP, the whole Ptr is written.
    // If has more than 1 GEP, bitcast/HLSubscript between GEPs, not support.
    // FIXME: support bitcast/HLSubscript case.
    if (Write.AddressList.size() != 1)
      return false;
    GEPOperator *GEP = dyn_cast<GEPOperator>(Write.AddressList.front());
    if (!GEP)
      return false;
    // bitcast before GEP, not support.
    if (GEP->getPointerOperand() != Ptr)
      return false;
    Slots.emplace_back(Write.cacluateSlots(DL));
  }
  // Sort slots by offset.
  std::sort(Slots.begin(), Slots.end(),
            [](WriteSlot &a, WriteSlot &b) { return a.Offset < b.Offset; });

  auto &EndSlot = Slots.back();
  if ((EndSlot.Offset + EndSlot.Size) != Size)
    return true;

  auto it = Slots.begin();

  if (it->Offset != 0)
    return true;
  unsigned CurOffset = it->Size;
  for (++it; it != Slots.end(); ++it) {
    // Space between slot.
    if (it->Offset > CurOffset)
      return true;

    CurOffset = it->Offset + it->Size;
  }
  return false;
}

struct RedundantPattern {
  MemCpyInst *UavLdCpy;
  MemCpyInst *UavStCpy;
  Value *Tmp;
  SmallVector<AllocaWrite, 4> AllocWrites;
  RedundantPattern(MemCpyInst *UavLd, MemCpyInst *UavSt, Value *Tmp)
      : UavLdCpy(UavLd), UavStCpy(UavSt), Tmp(Tmp) {}
  void doCutMemcpyToParitialWrite(const DataLayout &DL);
};

void RedundantPattern::doCutMemcpyToParitialWrite(const DataLayout &DL) {
  Value *UavDest = UavStCpy->getDest();
  IRBuilder<> B(UavStCpy);
  SmallVector<WriteSlot, 4> Slots;
  // FIXME: merge overlap writes.
  for (auto &Write : AllocWrites) {
    if (Write.Inst == UavLdCpy)
      continue;
    if (Write.Inst == UavStCpy)
      continue;
    // Assume all write have 1 GEP.
    DXASSERT(Write.AddressList.size() == 1,
             "isPartialWrite fail to confirm all write should have 1 GEP");
    GEPOperator *GEP = cast<GEPOperator>(Write.AddressList.front());
    SmallVector<Value *, 4> IdxList(GEP->idx_begin(), GEP->idx_end());
    Value *UavGEP = B.CreateGEP(UavDest, IdxList);
    Type *Ty = GEP->getType()->getPointerElementType();
    B.CreateMemCpy(UavGEP, GEP, DL.getTypeAllocSize(Ty), 1);
  }
  UavStCpy->eraseFromParent();
}

// collect uav access pattern
// Pattern is
//  Tmp = Uav0; // UavLdCpy
//  Update Tmp; // partial update.
//  Uav1 = Tmp; // UavStCpy
SmallVector<RedundantPattern, 4>
collectCandidates(Module &M, SmallDenseMap<Function *, DominatorTree> &DTMap,
                  SmallDenseMap<Function *, LoopInfo> &LoopInfoMap,
                  SmallDenseMap<Function *, SameLoopReachInfo> &ReachInfoMap,
                  SmallPtrSet<Type *, 4> &CandidateTypeSet,
                  DxilTypeSystem &typeSys) {
  auto &DL = M.getDataLayout();

  SmallVector<RedundantPattern, 4> Candidates;

  for (Function &F : M.functions()) {
    if (!F.isDeclaration())
      continue;

    // All resources with same return type will get same subscript overload.
    // Resource with different return type will not alias each other.
    SmallPtrSet<MemCpyInst *, 4> UavLoadSet;
    SmallPtrSet<MemCpyInst *, 4> UavStoreSet;

    for (User *U : F.users()) {
      CallInst *CI = dyn_cast<CallInst>(U);
      if (!CI)
        continue;
      // skip if not subscript.
      HLOpcodeGroup group = GetHLOpcodeGroupByName(&F);
      if (group != HLOpcodeGroup::HLSubscript)
        continue;

      unsigned opcode = GetHLOpcode(CI);
      HLSubscriptOpcode subOp = static_cast<HLSubscriptOpcode>(opcode);
      if (subOp != HLSubscriptOpcode::DefaultSubscript)
        continue;

      Value *Hdl = CI->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx);
      if (!isRWStructuredBuffer(Hdl))
        continue;

      Type *Ty = CI->getType();
      unsigned size = DL.getTypeAllocSize(Ty->getPointerElementType());
      hlutil::PointerStatus PS(CI, size, /*bLdStOnly*/ false);
      const bool bStructElt = false;
      PS.analyze(typeSys, bStructElt);
      if (PS.storedType == hlutil::PointerStatus::StoredType::MemcopyDestOnce) {
        UavStoreSet.insert(PS.StoringMemcpy);
      }
      if (PS.loadedType == hlutil::PointerStatus::LoadedType::MemcopySrcOnce) {
        UavLoadSet.insert(PS.LoadingMemcpy);
      }
    }
    if (UavStoreSet.empty())
      continue;

    Type *Ty = F.getReturnType();
    Ty = Ty->getPointerElementType();

    for (MemCpyInst *UavStCpy : UavStoreSet) {
      Value *Src = UavStCpy->getSource();
      // Only support alloc as temp.
      if (!isa<AllocaInst>(Src))
        continue;
      unsigned size = DL.getTypeAllocSize(Ty);
      hlutil::PointerStatus PS(Src, size, /*bLdStOnly*/ false);
      const bool bStructElt = false;
      PS.analyze(typeSys, bStructElt);

      if (PS.memcpySet.size() != 2)
        continue;

      auto mit = PS.memcpySet.begin();
      MemCpyInst *MC0 = *mit;
      mit++;
      MemCpyInst *MC1 = *mit;
      MemCpyInst *UavLdCpy = MC0 == UavStCpy ? MC1 : MC0;
      if (UavLoadSet.count(UavLdCpy) == 0)
        continue;

      // Pattern need is this.
      //  Tmp = UAV0; // UavLdCpy
      //  Update Tmp; // partial update.
      //  UAV1 = Tmp; // UavStCpy

      Function *F = UavLdCpy->getParent()->getParent();

      if (DTMap.find(F) == DTMap.end()) {
        DTMap[F].recalculate(*F);
      }
      auto &DT = DTMap[F];
      // Uav ld must dominate UavSt.
      if (!DT.dominates(UavLdCpy, UavStCpy))
        continue;

      if (LoopInfoMap.find(F) == LoopInfoMap.end()) {
        LoopInfoMap[F].Analyze(DT);
      }
      auto &LI = LoopInfoMap[F];
      // Uav ld/st must inside same loop.
      if (LI.getLoopFor(UavLdCpy->getParent()) !=
          LI.getLoopFor(UavStCpy->getParent()))
        continue;

      // Make sure UAV0 is same as UAV1.
      Value *Uav0 = UavLdCpy->getSource();
      Value *Uav1 = UavStCpy->getDest();
      if (!isSameUavSubScript(Uav0, Uav1, DT))
        continue;
      // Merge GEP to make collect easier.
      dxilutil::MergeGepUse(Src);

      if (ReachInfoMap.find(F) == ReachInfoMap.end()) {
        ReachInfoMap[F].build(*F);
      }
      auto &RI = ReachInfoMap[F];
      // UavLdCpy must dominate all user of Tmp.
      // Write of Tmp must before UavStCpy.
      RedundantPattern Redundant{UavLdCpy, UavStCpy, Src};
      SmallPtrSet<Value *, 4> Visited;
      SmallVector<Instruction *, 4> GEPList;
      collectAllocaWrites(Src, Redundant.AllocWrites, GEPList, Visited);
      if (!allAllocaWritesInBetween(Src, UavLdCpy, UavStCpy, DT, RI,
                                    Redundant.AllocWrites))
        continue;

      // Only partial write have redundant uav ld/st.
      if (!isPartialWrite(Src, M, UavLdCpy, UavStCpy, Redundant.AllocWrites))
        continue;

      CandidateTypeSet.insert(Ty);
      Candidates.emplace_back(Redundant);
    }
  }

  return Candidates;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
// For Pattern like
//  Tmp = uav[i]; // UavLdCpy
//  ...
//  Update Tmp; // partial update.
//  ...
//  uav[i] = Tmp; // UavStCpy
// Only the updated part need to store.
// The part which not updated will load and store the same value.
//
// RemoveRedundantUAVLdSt pass is to remove the redundant uav load store.
//
// RemoveRedundantUAVLdSt will identify the pattern before SROA, so memcpy is
// not lowered. Check the pattern on memcpy will be much easier and faster than
// check ld/st on all the elements.
//
// RemoveRedundantUAVLdSt is done after inline, where Tmp is not used as a
// argument for function call. If Tmp used as argument, it is hard to check
// partial updated.
//
// First identify all the candidates which match the pattern.
// Then build write info for Tmp.
// Finally change uav[i] = Tmp into uav[i].updated_part = Tmp.updated_part.
//
// After optimization it will be like
//  Tmp = uav[i]; // UavLdCpy
//  ...
//  Update Tmp; // partial update.
//  ...
//  uav[i].updated_part = Tmp.updated_part;
//
// This will remove the store on the part which not been updated.
// The load will be removed later in DCE if it is not used.
namespace {

class RemoveRedundantUAVLdSt : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit RemoveRedundantUAVLdSt() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "Remove redundant uav ld st";
  }

  bool runOnModule(Module &M) override {
    bool bUpdated = false;
    HLModule &HLM = M.GetOrCreateHLModule();
    // FIXME: support lib profile where extern function could be used.
    // For case like
    // RWStructuredBuffer<S> u;
    // void foo(inout S s);
    // float4 main(uint i:I) : SV_Target {
    //  foo(u[i]);
    //  return u[i].b;
    // }
    // Cannot remove the copy-in, copy-out.
    // Because cannot lower call on external function with uav subscript.
    //
    if (HLM.GetShaderModel()->IsLib())
      return bUpdated;
    SmallDenseMap<Function *, DominatorTree> DTMap;
    SmallDenseMap<Function *, LoopInfo> LoopInfoMap;
    SmallDenseMap<Function *, SameLoopReachInfo> ReachInfoMap;
    SmallPtrSet<Type *, 4> CandidateTypeSet;

    SmallVector<RedundantPattern, 4> Candidates =
        collectCandidates(M, DTMap, LoopInfoMap, ReachInfoMap, CandidateTypeSet,
                          HLM.GetTypeSystem());

    if (Candidates.empty())
      return bUpdated;

    auto UavAccess = collectUavAccess(M, CandidateTypeSet);
    auto &DL = M.getDataLayout();
    for (auto &Candidate : Candidates) {
      MemCpyInst *UavLdCpy = Candidate.UavLdCpy;
      MemCpyInst *UavStCpy = Candidate.UavStCpy;
      Type *Ty = UavStCpy->getDest()->getType()->getPointerElementType();
      UavAccessInfo &Info = UavAccess[Ty];
      // Reduce UavStCpy to only ld/st on the written part of Tmp.
      // Reduce UavLdCpy to only ld/st on the read of Tmp.
      Function *F = UavStCpy->getParent()->getParent();
      if (ReachInfoMap.find(F) == ReachInfoMap.end()) {
        ReachInfoMap[F].build(*F);
      }
      SameLoopReachInfo &ReachInfo = ReachInfoMap[F];
      // If there's uav access in between, the address might already be write to
      // different value. So the write is not redundant.
      if (!noAccessUAVInBetween(UavStCpy, UavLdCpy, Info, ReachInfo))
        continue;

      Candidate.doCutMemcpyToParitialWrite(DL);
      bUpdated = true;
    }

    return bUpdated;
  }
};

char RemoveRedundantUAVLdSt::ID = 0;

} // namespace

ModulePass *llvm::createRemoveRedundantUAVLdStPass() {
  return new RemoveRedundantUAVLdSt();
}

INITIALIZE_PASS(RemoveRedundantUAVLdSt, "hl-remove-redundant-uav-ldst",
                "Remove redundant uav ld st", false, false)
