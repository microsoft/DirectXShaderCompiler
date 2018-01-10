///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCondenseResources.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to make resource IDs zero-based and dense.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

struct ResourceID {
  DXIL::ResourceClass Class;  // Resource class.
  unsigned ID;                // Resource ID, as specified on entry.

  bool operator<(const ResourceID& other) const {
    if (Class < other.Class) return true;
    if (Class > other.Class) return false;
    if (ID < other.ID) return true;
    return false;
  }
};

struct RemapEntry {
  ResourceID ResID;           // Resource identity, as specified on entry.
  DxilResourceBase *Resource; // In-memory resource representation.
  unsigned Index;             // Index in resource vector - new ID for the resource.
};

typedef std::map<ResourceID, RemapEntry> RemapEntryCollection;

class DxilCondenseResources : public ModulePass {
private:
  RemapEntryCollection m_rewrites;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilCondenseResources() : ModulePass(ID) {}

  const char *getPassName() const override { return "DXIL Condense Resources"; }

  bool runOnModule(Module &M) override {
    DxilModule &DM = M.GetOrCreateDxilModule();

    // Switch tbuffers to SRVs, as they have been treated as cbuffers up to this point.
    if (DM.GetCBuffers().size())
      PatchTBuffers(DM);

    // Remove unused resource.
    DM.RemoveUnusedResources();

    // Make sure all resource types are dense; build a map of rewrites.
    if (BuildRewriteMap(DM)) {
      // Rewrite all instructions that refer to resources in the map.
      ApplyRewriteMap(DM);
    }

    bool hasResource = DM.GetCBuffers().size() ||
        DM.GetUAVs().size() || DM.GetSRVs().size() || DM.GetSamplers().size();

    if (hasResource) {
      if (!DM.GetShaderModel()->IsLib()) {
        AllocateDxilResources(DM);
        PatchCreateHandle(DM);
      } else {
        PatchCreateHandleForLib(DM);
      }
    }
    return true;
  }

  // Build m_rewrites, returns 'true' if any rewrites are needed.
  bool BuildRewriteMap(DxilModule &DM);

  DxilResourceBase &GetFirstRewrite() const {
    DXASSERT_NOMSG(!m_rewrites.empty());
    return *m_rewrites.begin()->second.Resource;
  }

private:
  void ApplyRewriteMap(DxilModule &DM);
  void AllocateDxilResources(DxilModule &DM);
  // Add lowbound to create handle range index.
  void PatchCreateHandle(DxilModule &DM);
  // Add lowbound to create handle range index for library.
  void PatchCreateHandleForLib(DxilModule &DM);
  // Switch CBuffer for SRV for TBuffers.
  void PatchTBuffers(DxilModule &DM);
};

void DxilCondenseResources::ApplyRewriteMap(DxilModule &DM) {
  for (Function &F : DM.GetModule()->functions()) {
    if (F.isDeclaration()) {
      continue;
    }

    for (inst_iterator iter = inst_begin(F), E = inst_end(F); iter != E; ++iter) {
      llvm::Instruction &I = *iter;
      DxilInst_CreateHandle CH(&I);
      if (!CH)
        continue;

      ResourceID RId;
      RId.Class = (DXIL::ResourceClass)CH.get_resourceClass_val();
      RId.ID = (unsigned)llvm::dyn_cast<llvm::ConstantInt>(CH.get_rangeId())
                   ->getZExtValue();
      RemapEntryCollection::iterator it = m_rewrites.find(RId);
      if (it == m_rewrites.end()) {
        continue;
      }

      CallInst *CI = cast<CallInst>(&I);
      Value *newRangeID = DM.GetOP()->GetU32Const(it->second.Index);
      CI->setArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx,
                        newRangeID);
    }
  }

  for (auto &entry : m_rewrites) {
    entry.second.Resource->SetID(entry.second.Index);
  }
}

template <typename TResource>
static void BuildRewrites(const std::vector<std::unique_ptr<TResource>> &Rs,
                          RemapEntryCollection &C) {
  const unsigned s = (unsigned)Rs.size();
  for (unsigned i = 0; i < s; ++i) {
    const std::unique_ptr<TResource> &R = Rs[i];
    if (R->GetID() != i) {
      ResourceID RId = {R->GetClass(), R->GetID()};
      RemapEntry RE = {RId, R.get(), i};
      C[RId] = RE;
    }
  }
}

bool DxilCondenseResources::BuildRewriteMap(DxilModule &DM) {
  BuildRewrites(DM.GetCBuffers(), m_rewrites);
  BuildRewrites(DM.GetSRVs(), m_rewrites);
  BuildRewrites(DM.GetUAVs(), m_rewrites);
  BuildRewrites(DM.GetSamplers(), m_rewrites);

  return !m_rewrites.empty();
}

namespace {

template<typename T>
static void AllocateDxilResource(const std::vector<std::unique_ptr<T> > &resourceList, LLVMContext &Ctx) {
  SpacesAllocator<unsigned, T> SAlloc;

  for (auto &res : resourceList) {
    const unsigned space = res->GetSpaceID();
    typename SpacesAllocator<unsigned, T>::Allocator &alloc = SAlloc.Get(space);

    if (res->IsAllocated()) {
      const unsigned reg = res->GetLowerBound();
      const T *conflict = nullptr;
      if (res->IsUnbounded()) {
        const T *unbounded = alloc.GetUnbounded();
        if (unbounded) {
          Ctx.emitError(
            Twine("more than one unbounded resource (") +
            unbounded->GetGlobalName() +
            (" and ") + res->GetGlobalName() +
            (") in space ") + Twine(space));
        } else {
          conflict = alloc.Insert(res.get(), reg, res->GetUpperBound());
          if (!conflict)
            alloc.SetUnbounded(res.get());
        }
      } else {
        conflict = alloc.Insert(res.get(), reg, res->GetUpperBound());
      }
      if (conflict) {
        Ctx.emitError(
          ((res->IsUnbounded()) ? Twine("unbounded ") : Twine("")) +
          Twine("resource ") + res->GetGlobalName() +
          Twine(" at register ") + Twine(reg) +
          Twine(" overlaps with resource ") + conflict->GetGlobalName() +
          Twine(" at register ") + Twine(conflict->GetLowerBound()) +
          Twine(", space ") + Twine(space));
      }
    }
  }

  // Allocate.
  const unsigned space = 0;
  typename SpacesAllocator<unsigned, T>::Allocator &alloc0 = SAlloc.Get(space);
  for (auto &res : resourceList) {
    if (!res->IsAllocated()) {
      DXASSERT(res->GetSpaceID() == 0, "otherwise non-zero space has no user register assignment");
      unsigned reg = 0;
      bool success = false;
      if (res->IsUnbounded()) {
        const T *unbounded = alloc0.GetUnbounded();
        if (unbounded) {
          Ctx.emitError(
            Twine("more than one unbounded resource (") +
            unbounded->GetGlobalName() +
            Twine(" and ") + res->GetGlobalName() +
            Twine(") in space ") + Twine(space));
        } else {
          success = alloc0.AllocateUnbounded(res.get(), reg);
          if (success)
            alloc0.SetUnbounded(res.get());
        }
      } else {
        success = alloc0.Allocate(res.get(), res->GetRangeSize(), reg);
      }
      if (success) {
        res->SetLowerBound(reg);
      } else {
        Ctx.emitError(
          ((res->IsUnbounded()) ? Twine("unbounded ") : Twine("")) +
          Twine("resource ") + res->GetGlobalName() +
          Twine(" could not be allocated"));
      }
    }
  }
}

void PatchLowerBoundOfCreateHandle(CallInst *handle, DxilModule &DM) {
  DxilInst_CreateHandle createHandle(handle);
  DXASSERT_NOMSG(createHandle);

  DXIL::ResourceClass ResClass =
      static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val());
  // Dynamic rangeId is not supported - skip and let validation report the
  // error.
  if (!isa<ConstantInt>(createHandle.get_rangeId()))
    return;

  unsigned rangeId =
      cast<ConstantInt>(createHandle.get_rangeId())->getLimitedValue();

  DxilResourceBase *res = nullptr;
  switch (ResClass) {
  case DXIL::ResourceClass::SRV:
    res = &DM.GetSRV(rangeId);
    break;
  case DXIL::ResourceClass::UAV:
    res = &DM.GetUAV(rangeId);
    break;
  case DXIL::ResourceClass::CBuffer:
    res = &DM.GetCBuffer(rangeId);
    break;
  case DXIL::ResourceClass::Sampler:
    res = &DM.GetSampler(rangeId);
    break;
  default:
    DXASSERT(0, "invalid res class");
    return;
  }
  IRBuilder<> Builder(handle);
  unsigned lowBound = res->GetLowerBound();
  if (lowBound) {
    Value *Index = createHandle.get_index();
    if (ConstantInt *cIndex = dyn_cast<ConstantInt>(Index)) {
      unsigned newIdx = lowBound + cIndex->getLimitedValue();
      handle->setArgOperand(DXIL::OperandIndex::kCreateHandleResIndexOpIdx,
                            Builder.getInt32(newIdx));
    } else {
      Value *newIdx = Builder.CreateAdd(Index, Builder.getInt32(lowBound));
      handle->setArgOperand(DXIL::OperandIndex::kCreateHandleResIndexOpIdx,
                            newIdx);
    }
  }
}

static void PatchTBufferCreateHandle(CallInst *handle, DxilModule &DM, std::unordered_set<unsigned> &tbufferIDs) {
  DxilInst_CreateHandle createHandle(handle);
  DXASSERT_NOMSG(createHandle);

  DXIL::ResourceClass ResClass = static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val());
  if (ResClass != DXIL::ResourceClass::CBuffer)
    return;

  Value *resID = createHandle.get_rangeId();
  DXASSERT(isa<ConstantInt>(resID), "cannot handle dynamic resID for cbuffer CreateHandle");
  if (!isa<ConstantInt>(resID))
    return;

  unsigned rangeId = cast<ConstantInt>(resID)->getLimitedValue();
  DxilResourceBase *res = &DM.GetCBuffer(rangeId);

  // For TBuffer, we need to switch resource type from CBuffer to SRV
  if (res->GetKind() == DXIL::ResourceKind::TBuffer) {
    // Track cbuffers IDs that are actually tbuffers
    tbufferIDs.insert(rangeId);
    hlsl::OP *hlslOP = DM.GetOP();
    llvm::LLVMContext &Ctx = DM.GetCtx();

    // Temporarily add SRV size to rangeID to guarantee unique new SRV ID
    Value *newRangeID = hlslOP->GetU32Const(rangeId + DM.GetSRVs().size());
    handle->setArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx,
                          newRangeID);
    // switch create handle to SRV
    handle->setArgOperand(DXIL::OperandIndex::kCreateHandleResClassOpIdx,
                          hlslOP->GetU8Const(
                            static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
                              DXIL::ResourceClass::SRV)));

    Type *doubleTy = Type::getDoubleTy(Ctx);
    Type *i64Ty = Type::getInt64Ty(Ctx);

    // Replace corresponding cbuffer loads with typed buffer loads
    for (auto U = handle->user_begin(); U != handle->user_end(); ) {
      CallInst *I = cast<CallInst>(*(U++));
      DXASSERT(I && OP::IsDxilOpFuncCallInst(I), "otherwise unexpected user of CreateHandle value");
      DXIL::OpCode opcode = OP::GetDxilOpFuncCallInst(I);
      if (opcode == DXIL::OpCode::CBufferLoadLegacy) {
        DxilInst_CBufferLoadLegacy cbLoad(I);

        // Replace with appropriate buffer load instruction
        IRBuilder<> Builder(I);
        opcode = OP::OpCode::BufferLoad;
        Type *Ty = Type::getInt32Ty(Ctx);
        Function *BufLoad = hlslOP->GetOpFunc(opcode, Ty);
        Constant *opArg = hlslOP->GetU32Const((unsigned)opcode);
        Value *undefI = UndefValue::get(Type::getInt32Ty(Ctx));
        Value *offset = cbLoad.get_regIndex();
        CallInst* load = Builder.CreateCall(BufLoad, {opArg, handle, offset, undefI});

        // Find extractelement uses of cbuffer load and replace + generate bitcast as necessary
        for (auto LU = I->user_begin(); LU != I->user_end(); ) {
          ExtractValueInst *evInst = dyn_cast<ExtractValueInst>(*(LU++));
          DXASSERT(evInst && evInst->getNumIndices() == 1, "user of cbuffer load result should be extractvalue");
          uint64_t idx = evInst->getIndices()[0];
          Type *EltTy = evInst->getType();
          IRBuilder<> EEBuilder(evInst);
          Value *result = nullptr;
          if (EltTy != Ty) {
            // extract two values and DXIL::OpCode::MakeDouble or construct i64
            if ((EltTy == doubleTy) || (EltTy == i64Ty)) {
              DXASSERT(idx < 2, "64-bit component index out of range");

              // This assumes big endian order in tbuffer elements (is this correct?)
              Value *low = EEBuilder.CreateExtractValue(load, idx * 2);
              Value *high = EEBuilder.CreateExtractValue(load, idx * 2 + 1);
              if (EltTy == doubleTy) {
                opcode = OP::OpCode::MakeDouble;
                Function *MakeDouble = hlslOP->GetOpFunc(opcode, doubleTy);
                Constant *opArg = hlslOP->GetU32Const((unsigned)opcode);
                result = EEBuilder.CreateCall(MakeDouble, {opArg, low, high});
              } else {
                high = EEBuilder.CreateZExt(high, i64Ty);
                low = EEBuilder.CreateZExt(low, i64Ty);
                high = EEBuilder.CreateShl(high, hlslOP->GetU64Const(32));
                result = EEBuilder.CreateOr(high, low);
              }
            } else {
              result = EEBuilder.CreateExtractValue(load, idx);
              result = EEBuilder.CreateBitCast(result, EltTy);
            }
          } else {
            result = EEBuilder.CreateExtractValue(load, idx);
          }

          evInst->replaceAllUsesWith(result);
          evInst->eraseFromParent();
        }
      } else if (opcode == DXIL::OpCode::CBufferLoad) {
        // TODO: Handle this, or prevent this for tbuffer
        DXASSERT(false, "otherwise CBufferLoad used for tbuffer rather than CBufferLoadLegacy");
      } else {
        DXASSERT(false, "otherwise unexpected user of CreateHandle value");
      }
      I->eraseFromParent();
    }
  }
}

}


void DxilCondenseResources::AllocateDxilResources(DxilModule &DM) {
  AllocateDxilResource(DM.GetCBuffers(), DM.GetCtx());
  AllocateDxilResource(DM.GetSamplers(), DM.GetCtx());
  AllocateDxilResource(DM.GetUAVs(), DM.GetCtx());
  AllocateDxilResource(DM.GetSRVs(), DM.GetCtx());
}

void InitTBuffer(const DxilCBuffer *pSource, DxilResource *pDest) {
  pDest->SetKind(pSource->GetKind());
  pDest->SetCompType(DXIL::ComponentType::U32);
  pDest->SetSampleCount(0);
  pDest->SetElementStride(0);
  pDest->SetGloballyCoherent(false);
  pDest->SetHasCounter(false);
  pDest->SetRW(false);
  pDest->SetROV(false);
  pDest->SetID(pSource->GetID());
  pDest->SetSpaceID(pSource->GetSpaceID());
  pDest->SetLowerBound(pSource->GetLowerBound());
  pDest->SetRangeSize(pSource->GetRangeSize());
  pDest->SetGlobalSymbol(pSource->GetGlobalSymbol());
  pDest->SetGlobalName(pSource->GetGlobalName());
  pDest->SetHandle(pSource->GetHandle());
}

void DxilCondenseResources::PatchTBuffers(DxilModule &DM) {
  Function *createHandle = DM.GetOP()->GetOpFunc(DXIL::OpCode::CreateHandle,
                                                 Type::getVoidTy(DM.GetCtx()));

  std::unordered_set<unsigned> tbufferIDs;
  for (User *U : createHandle->users()) {
    PatchTBufferCreateHandle(cast<CallInst>(U), DM, tbufferIDs);
  }

  // move tbuffer resources to SRVs
  unsigned offset = DM.GetSRVs().size();
  for (auto it = DM.GetCBuffers().begin(); it != DM.GetCBuffers().end(); it++) {
    DxilCBuffer *CB = it->get();
    unsigned resID = CB->GetID();
    if (tbufferIDs.find(resID) != tbufferIDs.end()) {
      auto srv = make_unique<DxilResource>();
      InitTBuffer(CB, srv.get());
      srv->SetID(resID + offset);
      DM.AddSRV(std::move(srv));
      // cbuffer should get cleaned up since it's now unused.
    }
  }
}

void DxilCondenseResources::PatchCreateHandle(DxilModule &DM) {
  Function *createHandle = DM.GetOP()->GetOpFunc(DXIL::OpCode::CreateHandle,
                                                 Type::getVoidTy(DM.GetCtx()));

  for (User *U : createHandle->users()) {
    PatchLowerBoundOfCreateHandle(cast<CallInst>(U), DM);
  }
}

static Value *PatchRangeIDForLib(DxilModule &DM, IRBuilder<> &Builder,
                                 Value *rangeIdVal,
                                 std::unordered_map<PHINode *, Value *> &phiMap,
                                 DXIL::ResourceClass ResClass) {
  Value *linkRangeID = nullptr;
  if (isa<ConstantInt>(rangeIdVal)) {
    unsigned rangeId = cast<ConstantInt>(rangeIdVal)->getLimitedValue();

    const DxilModule::ResourceLinkInfo &linkInfo =
        DM.GetResourceLinkInfo(ResClass, rangeId);
    linkRangeID = Builder.CreateLoad(linkInfo.ResRangeID);
  } else {
    if (PHINode *phi = dyn_cast<PHINode>(rangeIdVal)) {
      auto it = phiMap.find(phi);
      if (it == phiMap.end()) {
        unsigned numOperands = phi->getNumOperands();

        PHINode *phiRangeID = Builder.CreatePHI(phi->getType(), numOperands);
        phiMap[phi] = phiRangeID;

        std::vector<Value *> rangeIDs(numOperands);
        for (unsigned i = 0; i < numOperands; i++) {
          Value *V = phi->getOperand(i);
          BasicBlock *BB = phi->getIncomingBlock(i);
          IRBuilder<> Builder(BB->getTerminator());
          rangeIDs[i] = PatchRangeIDForLib(DM, Builder, V, phiMap, ResClass);
        }

        for (unsigned i = 0; i < numOperands; i++) {
          Value *V = rangeIDs[i];
          BasicBlock *BB = phi->getIncomingBlock(i);
          phiRangeID->addIncoming(V, BB);
        }
        linkRangeID = phiRangeID;
      } else {
        linkRangeID = it->second;
      }
    } else if (SelectInst *si = dyn_cast<SelectInst>(rangeIdVal)) {
      IRBuilder<> Builder(si);
      Value *trueVal =
          PatchRangeIDForLib(DM, Builder, si->getTrueValue(), phiMap, ResClass);
      Value *falseVal = PatchRangeIDForLib(DM, Builder, si->getFalseValue(),
                                           phiMap, ResClass);
      linkRangeID = Builder.CreateSelect(si->getCondition(), trueVal, falseVal);
    } else if (CastInst *cast = dyn_cast<CastInst>(rangeIdVal)) {
      if (cast->getOpcode() == CastInst::CastOps::ZExt &&
          cast->getOperand(0)->getType() == Type::getInt1Ty(DM.GetCtx())) {
        // select cond, 1, 0.
        IRBuilder<> Builder(cast);
        Value *trueVal = PatchRangeIDForLib(
            DM, Builder, ConstantInt::get(cast->getType(), 1), phiMap,
            ResClass);
        Value *falseVal = PatchRangeIDForLib(
            DM, Builder, ConstantInt::get(cast->getType(), 0), phiMap,
            ResClass);
        linkRangeID =
            Builder.CreateSelect(cast->getOperand(0), trueVal, falseVal);
      }
    }
  }
  return linkRangeID;
}

void DxilCondenseResources::PatchCreateHandleForLib(DxilModule &DM) {
  Function *createHandle = DM.GetOP()->GetOpFunc(DXIL::OpCode::CreateHandle,
                                                 Type::getVoidTy(DM.GetCtx()));
  DM.CreateResourceLinkInfo();
  for (User *U : createHandle->users()) {
    CallInst *handle = cast<CallInst>(U);
    DxilInst_CreateHandle createHandle(handle);
    DXASSERT_NOMSG(createHandle);

    DXIL::ResourceClass ResClass =
        static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val());

    std::unordered_map<PHINode *, Value*> phiMap;
    Value *rangeID = createHandle.get_rangeId();
    IRBuilder<> Builder(handle);
    Value *linkRangeID = PatchRangeIDForLib(
        DM, Builder, rangeID, phiMap, ResClass);

    // Dynamic rangeId is not supported - skip and let validation report the
    // error.
    if (!linkRangeID)
      continue;
    // Update rangeID to linkinfo rangeID.
    handle->setArgOperand(DXIL::OperandIndex::kCreateHandleResIDOpIdx,
                          linkRangeID);
    if (rangeID->user_empty() && isa<Instruction>(rangeID)) {
      cast<Instruction>(rangeID)->eraseFromParent();
    }
  }
}

char DxilCondenseResources::ID = 0;

bool llvm::AreDxilResourcesDense(llvm::Module *M, hlsl::DxilResourceBase **ppNonDense) {
  DxilModule &DM = M->GetOrCreateDxilModule();
  DxilCondenseResources Pass;
  if (Pass.BuildRewriteMap(DM)) {
    *ppNonDense = &Pass.GetFirstRewrite();
    return false;
  }
  else {
    *ppNonDense = nullptr;
    return true;
  }
}

ModulePass *llvm::createDxilCondenseResourcesPass() {
  return new DxilCondenseResources();
}

INITIALIZE_PASS(DxilCondenseResources, "hlsl-dxil-condense", "DXIL Condense Resources", false, false)
