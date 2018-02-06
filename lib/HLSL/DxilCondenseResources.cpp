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
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/DxilUtil.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

// Resource rangeID remap.
namespace {
struct ResourceID {
  DXIL::ResourceClass Class; // Resource class.
  unsigned ID;               // Resource ID, as specified on entry.

  bool operator<(const ResourceID &other) const {
    if (Class < other.Class)
      return true;
    if (Class > other.Class)
      return false;
    if (ID < other.ID)
      return true;
    return false;
  }
};

struct RemapEntry {
  ResourceID ResID;           // Resource identity, as specified on entry.
  DxilResourceBase *Resource; // In-memory resource representation.
  unsigned Index; // Index in resource vector - new ID for the resource.
};

typedef std::map<ResourceID, RemapEntry> RemapEntryCollection;

template <typename TResource>
void BuildRewrites(const std::vector<std::unique_ptr<TResource>> &Rs,
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

// Build m_rewrites, returns 'true' if any rewrites are needed.
bool BuildRewriteMap(RemapEntryCollection &rewrites, DxilModule &DM) {
  BuildRewrites(DM.GetCBuffers(), rewrites);
  BuildRewrites(DM.GetSRVs(), rewrites);
  BuildRewrites(DM.GetUAVs(), rewrites);
  BuildRewrites(DM.GetSamplers(), rewrites);

  return !rewrites.empty();
}

void ApplyRewriteMapOnResTable(RemapEntryCollection &rewrites, DxilModule &DM) {
  for (auto &entry : rewrites) {
    entry.second.Resource->SetID(entry.second.Index);
  }
}

} // namespace

// Resource lowerBound allocation.
namespace {

template <typename T>
static void
AllocateDxilResource(const std::vector<std::unique_ptr<T>> &resourceList,
                     LLVMContext &Ctx) {
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
          Ctx.emitError(Twine("more than one unbounded resource (") +
                        unbounded->GetGlobalName() + (" and ") +
                        res->GetGlobalName() + (") in space ") + Twine(space));
        } else {
          conflict = alloc.Insert(res.get(), reg, res->GetUpperBound());
          if (!conflict)
            alloc.SetUnbounded(res.get());
        }
      } else {
        conflict = alloc.Insert(res.get(), reg, res->GetUpperBound());
      }
      if (conflict) {
        Ctx.emitError(((res->IsUnbounded()) ? Twine("unbounded ") : Twine("")) +
                      Twine("resource ") + res->GetGlobalName() +
                      Twine(" at register ") + Twine(reg) +
                      Twine(" overlaps with resource ") +
                      conflict->GetGlobalName() + Twine(" at register ") +
                      Twine(conflict->GetLowerBound()) + Twine(", space ") +
                      Twine(space));
      }
    }
  }

  // Allocate.
  const unsigned space = 0;
  typename SpacesAllocator<unsigned, T>::Allocator &alloc0 = SAlloc.Get(space);
  for (auto &res : resourceList) {
    if (!res->IsAllocated()) {
      DXASSERT(res->GetSpaceID() == 0,
               "otherwise non-zero space has no user register assignment");
      unsigned reg = 0;
      bool success = false;
      if (res->IsUnbounded()) {
        const T *unbounded = alloc0.GetUnbounded();
        if (unbounded) {
          Ctx.emitError(Twine("more than one unbounded resource (") +
                        unbounded->GetGlobalName() + Twine(" and ") +
                        res->GetGlobalName() + Twine(") in space ") +
                        Twine(space));
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
        Ctx.emitError(((res->IsUnbounded()) ? Twine("unbounded ") : Twine("")) +
                      Twine("resource ") + res->GetGlobalName() +
                      Twine(" could not be allocated"));
      }
    }
  }
}

void AllocateDxilResources(DxilModule &DM) {
  AllocateDxilResource(DM.GetCBuffers(), DM.GetCtx());
  AllocateDxilResource(DM.GetSamplers(), DM.GetCtx());
  AllocateDxilResource(DM.GetUAVs(), DM.GetCtx());
  AllocateDxilResource(DM.GetSRVs(), DM.GetCtx());
}
} // namespace

class DxilCondenseResources : public ModulePass {
private:
  RemapEntryCollection m_rewrites;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilCondenseResources() : ModulePass(ID) {}

  const char *getPassName() const override { return "DXIL Condense Resources"; }

  bool runOnModule(Module &M) override {
    DxilModule &DM = M.GetOrCreateDxilModule();

    // Remove unused resource.
    DM.RemoveUnusedResources();

    // Make sure all resource types are dense; build a map of rewrites.
    if (BuildRewriteMap(m_rewrites, DM)) {
      // Rewrite all instructions that refer to resources in the map.
      ApplyRewriteMap(DM);
    }

    bool hasResource = DM.GetCBuffers().size() ||
        DM.GetUAVs().size() || DM.GetSRVs().size() || DM.GetSamplers().size();

    if (hasResource) {
      if (!DM.GetShaderModel()->IsLib()) {
        AllocateDxilResources(DM);
        PatchCreateHandle(DM);
      }
    }
    return true;
  }

  DxilResourceBase &GetFirstRewrite() const {
    DXASSERT_NOMSG(!m_rewrites.empty());
    return *m_rewrites.begin()->second.Resource;
  }

private:
  void ApplyRewriteMap(DxilModule &DM);
  // Add lowbound to create handle range index.
  void PatchCreateHandle(DxilModule &DM);
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

  ApplyRewriteMapOnResTable(m_rewrites, DM);
}

namespace {

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

void DxilCondenseResources::PatchCreateHandle(DxilModule &DM) {
  Function *createHandle = DM.GetOP()->GetOpFunc(DXIL::OpCode::CreateHandle,
                                                 Type::getVoidTy(DM.GetCtx()));

  for (User *U : createHandle->users()) {
    PatchLowerBoundOfCreateHandle(cast<CallInst>(U), DM);
  }
}

char DxilCondenseResources::ID = 0;

bool llvm::AreDxilResourcesDense(llvm::Module *M, hlsl::DxilResourceBase **ppNonDense) {
  DxilModule &DM = M->GetOrCreateDxilModule();
  RemapEntryCollection rewrites;
  if (BuildRewriteMap(rewrites, DM)) {
    *ppNonDense = rewrites.begin()->second.Resource;
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

namespace {
class DxilLowerCreateHandleForLib : public ModulePass {
private:
  RemapEntryCollection m_rewrites;
  DxilModule *m_DM;
  bool m_HasDbgInfo;
  bool m_bIsLib;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLowerCreateHandleForLib() : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Lower createHandleForLib";
  }

  bool runOnModule(Module &M) override {
    DxilModule &DM = M.GetOrCreateDxilModule();
    m_DM = &DM;
    m_bIsLib = DM.GetShaderModel()->IsLib();

    // Switch tbuffers to SRVs, as they have been treated as cbuffers up to this
    // point.
    if (DM.GetCBuffers().size())
      PatchTBuffers(DM);

    // Remove unused resource.
    DM.RemoveUnusedResourceSymbols();

    bool hasResource = DM.GetCBuffers().size() || DM.GetUAVs().size() ||
                       DM.GetSRVs().size() || DM.GetSamplers().size();

    if (!hasResource || m_bIsLib)
      return false;

    BuildRewriteMap(m_rewrites, DM);
    ApplyRewriteMapOnResTable(m_rewrites, DM);

    // Load up debug information, to cross-reference values and the instructions
    // used to load them.
    m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

    AllocateDxilResources(DM);

    GenerateDxilCBufferHandles();
    GenerateDxilResourceHandles();
    AddCreateHandleForPhiNodeAndSelect(DM.GetOP());

    if (DM.GetOP()->UseMinPrecision())
      UpdateStructTypeForLegacyLayout();
    // Change resource symbol into undef.
    UpdateResourceSymbols();

    // Remove unused createHandleForLib functions.
    dxilutil::RemoveUnusedFunctions(M, DM.GetEntryFunction(),
                                    DM.GetPatchConstantFunction(), m_bIsLib);

    return true;
  }

private:
  void UpdateResourceSymbols();
  void TranslateDxilResourceUses(DxilResourceBase &res);
  void GenerateDxilResourceHandles();
  void AddCreateHandleForPhiNodeAndSelect(OP *hlslOP);
  // Generate DXIL cbuffer handles.
  void GenerateDxilCBufferHandles();
  void UpdateStructTypeForLegacyLayout();
  // Switch CBuffer for SRV for TBuffers.
  void PatchTBuffers(DxilModule &DM);
  void PatchTBufferUse(Value *V, DxilModule &DM);
};

// LegacyLayout.
namespace {

StructType *UpdateStructTypeForLegacyLayout(StructType *ST, bool IsCBuf,
                                            DxilTypeSystem &TypeSys, Module &M);

Type *UpdateFieldTypeForLegacyLayout(Type *Ty, bool IsCBuf,
                                     DxilFieldAnnotation &annotation,
                                     DxilTypeSystem &TypeSys, Module &M) {
  DXASSERT(!Ty->isPointerTy(), "struct field should not be a pointer");

  if (Ty->isArrayTy()) {
    Type *EltTy = Ty->getArrayElementType();
    Type *UpdatedTy =
        UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    if (EltTy == UpdatedTy)
      return Ty;
    else
      return ArrayType::get(UpdatedTy, Ty->getArrayNumElements());
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    DXASSERT(annotation.HasMatrixAnnotation(), "must a matrix");
    unsigned rows, cols;
    Type *EltTy = HLMatrixLower::GetMatrixInfo(Ty, cols, rows);

    // Get cols and rows from annotation.
    const DxilMatrixAnnotation &matrix = annotation.GetMatrixAnnotation();
    if (matrix.Orientation == MatrixOrientation::RowMajor) {
      rows = matrix.Rows;
      cols = matrix.Cols;
    } else {
      DXASSERT(matrix.Orientation == MatrixOrientation::ColumnMajor, "");
      cols = matrix.Rows;
      rows = matrix.Cols;
    }
    // CBuffer matrix must 4 * 4 bytes align.
    if (IsCBuf)
      cols = 4;

    EltTy =
        UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    Type *rowTy = VectorType::get(EltTy, cols);
    return ArrayType::get(rowTy, rows);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    return UpdateStructTypeForLegacyLayout(ST, IsCBuf, TypeSys, M);
  } else if (Ty->isVectorTy()) {
    Type *EltTy = Ty->getVectorElementType();
    Type *UpdatedTy =
        UpdateFieldTypeForLegacyLayout(EltTy, IsCBuf, annotation, TypeSys, M);
    if (EltTy == UpdatedTy)
      return Ty;
    else
      return VectorType::get(UpdatedTy, Ty->getVectorNumElements());
  } else {
    Type *i32Ty = Type::getInt32Ty(Ty->getContext());
    // Basic types.
    if (Ty->isHalfTy()) {
      return Type::getFloatTy(Ty->getContext());
    } else if (IntegerType *ITy = dyn_cast<IntegerType>(Ty)) {
      if (ITy->getBitWidth() < 32)
        return i32Ty;
      else
        return Ty;
    } else
      return Ty;
  }
}

StructType *UpdateStructTypeForLegacyLayout(StructType *ST, bool IsCBuf,
                                            DxilTypeSystem &TypeSys,
                                            Module &M) {
  bool bUpdated = false;
  unsigned fieldsCount = ST->getNumElements();
  std::vector<Type *> fieldTypes(fieldsCount);
  DxilStructAnnotation *SA = TypeSys.GetStructAnnotation(ST);
  DXASSERT(SA, "must have annotation for struct type");

  for (unsigned i = 0; i < fieldsCount; i++) {
    Type *EltTy = ST->getElementType(i);
    Type *UpdatedTy = UpdateFieldTypeForLegacyLayout(
        EltTy, IsCBuf, SA->GetFieldAnnotation(i), TypeSys, M);
    fieldTypes[i] = UpdatedTy;
    if (EltTy != UpdatedTy)
      bUpdated = true;
  }

  if (!bUpdated) {
    return ST;
  } else {
    std::string legacyName = "dx.alignment.legacy." + ST->getName().str();
    if (StructType *legacyST = M.getTypeByName(legacyName))
      return legacyST;

    StructType *NewST =
        StructType::create(ST->getContext(), fieldTypes, legacyName);
    DxilStructAnnotation *NewSA = TypeSys.AddStructAnnotation(NewST);
    // Clone annotation.
    *NewSA = *SA;
    return NewST;
  }
}

void UpdateStructTypeForLegacyLayout(DxilResourceBase &Res,
                                     DxilTypeSystem &TypeSys, Module &M) {
  GlobalVariable *GV = cast<GlobalVariable>(Res.GetGlobalSymbol());
  Type *Ty = GV->getType()->getPointerElementType();
  bool IsResourceArray = Res.GetRangeSize() != 1;
  if (IsResourceArray) {
    // Support Array of struct buffer.
    if (Ty->isArrayTy())
      Ty = Ty->getArrayElementType();
  }
  StructType *ST = cast<StructType>(Ty);
  if (ST->isOpaque()) {
    DXASSERT(Res.GetClass() == DxilResourceBase::Class::CBuffer,
             "Only cbuffer can have opaque struct.");
    return;
  }

  Type *UpdatedST =
      UpdateStructTypeForLegacyLayout(ST, IsResourceArray, TypeSys, M);
  if (ST != UpdatedST) {
    Type *Ty = GV->getType()->getPointerElementType();
    if (IsResourceArray) {
      // Support Array of struct buffer.
      if (Ty->isArrayTy()) {
        UpdatedST = ArrayType::get(UpdatedST, Ty->getArrayNumElements());
      }
    }
    GlobalVariable *NewGV = cast<GlobalVariable>(
        M.getOrInsertGlobal(GV->getName().str() + "_legacy", UpdatedST));
    Res.SetGlobalSymbol(NewGV);
    // Delete old GV.
    for (auto UserIt = GV->user_begin(); UserIt != GV->user_end();) {
      Value *User = *(UserIt++);
      if (Instruction *I = dyn_cast<Instruction>(User)) {
        if (!User->user_empty())
          I->replaceAllUsesWith(UndefValue::get(I->getType()));

        I->eraseFromParent();
      } else {
        ConstantExpr *CE = cast<ConstantExpr>(User);
        if (!CE->user_empty())
          CE->replaceAllUsesWith(UndefValue::get(CE->getType()));
      }
    }
    GV->removeDeadConstantUsers();
    GV->eraseFromParent();
  }
}

void UpdateStructTypeForLegacyLayoutOnDM(DxilModule &DM) {
  DxilTypeSystem &TypeSys = DM.GetTypeSystem();
  Module &M = *DM.GetModule();
  for (auto &CBuf : DM.GetCBuffers()) {
    UpdateStructTypeForLegacyLayout(*CBuf.get(), TypeSys, M);
  }

  for (auto &UAV : DM.GetUAVs()) {
    if (UAV->GetKind() == DxilResourceBase::Kind::StructuredBuffer)
      UpdateStructTypeForLegacyLayout(*UAV.get(), TypeSys, M);
  }

  for (auto &SRV : DM.GetSRVs()) {
    if (SRV->GetKind() == DxilResourceBase::Kind::StructuredBuffer)
      UpdateStructTypeForLegacyLayout(*SRV.get(), TypeSys, M);
  }
}

} // namespace

void DxilLowerCreateHandleForLib::UpdateStructTypeForLegacyLayout() {
  UpdateStructTypeForLegacyLayoutOnDM(*m_DM);
}

// Change ResourceSymbol to undef if don't need.
void DxilLowerCreateHandleForLib::UpdateResourceSymbols() {
  std::vector<GlobalVariable *> &LLVMUsed = m_DM->GetLLVMUsed();

  auto UpdateResourceSymbol = [&LLVMUsed, this](DxilResourceBase *res) {
    GlobalVariable *GV = cast<GlobalVariable>(res->GetGlobalSymbol());
    GV->removeDeadConstantUsers();
    DXASSERT(GV->user_empty(), "else resource not lowered");
    Type *Ty = GV->getType();
    res->SetGlobalSymbol(UndefValue::get(Ty));
    if (m_HasDbgInfo)
      LLVMUsed.emplace_back(GV);

    res->SetGlobalSymbol(UndefValue::get(Ty));
  };

  for (auto &&C : m_DM->GetCBuffers()) {
    UpdateResourceSymbol(C.get());
  }
  for (auto &&Srv : m_DM->GetSRVs()) {
    UpdateResourceSymbol(Srv.get());
  }
  for (auto &&Uav : m_DM->GetUAVs()) {
    UpdateResourceSymbol(Uav.get());
  }
  for (auto &&S : m_DM->GetSamplers()) {
    UpdateResourceSymbol(S.get());
  }
}

// Lower createHandleForLib
namespace {

void ReplaceResourceUserWithHandle(LoadInst *Res, Value *handle) {
  for (auto resUser = Res->user_begin(); resUser != Res->user_end();) {
    CallInst *CI = dyn_cast<CallInst>(*(resUser++));
    DxilInst_CreateHandleFromResourceStructForLib createHandle(CI);

    DXASSERT(createHandle, "must be createHandle");
    CI->replaceAllUsesWith(handle);
    CI->eraseFromParent();
  }
  Res->eraseFromParent();
}

DIGlobalVariable *FindGlobalVariableDebugInfo(GlobalVariable *GV,
                                              DebugInfoFinder &DbgInfoFinder) {
  struct GlobalFinder {
    GlobalVariable *GV;
    bool operator()(llvm::DIGlobalVariable *const arg) const {
      return arg->getVariable() == GV;
    }
  };
  GlobalFinder F = {GV};
  DebugInfoFinder::global_variable_iterator Found =
      std::find_if(DbgInfoFinder.global_variables().begin(),
                   DbgInfoFinder.global_variables().end(), F);
  if (Found != DbgInfoFinder.global_variables().end()) {
    return *Found;
  }
  return nullptr;
}
} // namespace
void DxilLowerCreateHandleForLib::TranslateDxilResourceUses(
    DxilResourceBase &res) {
  OP *hlslOP = m_DM->GetOP();
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(m_DM->GetCtx()));
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);
  bool isViewResource = res.GetClass() == DXIL::ResourceClass::SRV ||
                        res.GetClass() == DXIL::ResourceClass::UAV;
  bool isROV = isViewResource && static_cast<DxilResource &>(res).IsROV();
  std::string handleName =
      (res.GetGlobalName() + Twine("_") + Twine(res.GetResClassName())).str();
  if (isViewResource)
    handleName += (Twine("_") + Twine(res.GetResDimName())).str();
  if (isROV)
    handleName += "_ROV";

  Value *resClassArg = hlslOP->GetU8Const(
      static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
          res.GetClass()));
  Value *resIDArg = hlslOP->GetU32Const(res.GetID());
  // resLowerBound will be added after allocation in DxilCondenseResources.
  Value *resLowerBound = hlslOP->GetU32Const(res.GetLowerBound());
  // TODO: Set Non-uniform resource bit based on whether index comes from
  // IOP_NonUniformResourceIndex.
  Value *isUniformRes = hlslOP->GetI1Const(0);

  Value *GV = res.GetGlobalSymbol();
  Module *pM = m_DM->GetModule();
  // TODO: add debug info to create handle.
  DIVariable *DIV = nullptr;
  DILocation *DL = nullptr;
  if (m_HasDbgInfo) {
    DebugInfoFinder &Finder = m_DM->GetOrCreateDebugInfoFinder();
    DIV = FindGlobalVariableDebugInfo(cast<GlobalVariable>(GV), Finder);
    if (DIV)
      // TODO: how to get col?
      DL =
          DILocation::get(pM->getContext(), DIV->getLine(), 1, DIV->getScope());
  }

  bool isResArray = res.GetRangeSize() > 1;
  std::unordered_map<Function *, Instruction *> handleMapOnFunction;

  Value *createHandleArgs[] = {opArg, resClassArg, resIDArg, resLowerBound,
                               isUniformRes};

  for (iplist<Function>::iterator F : pM->getFunctionList()) {
    if (!F->isDeclaration()) {
      if (!isResArray) {
        IRBuilder<> Builder(F->getEntryBlock().getFirstInsertionPt());
        if (m_HasDbgInfo) {
          // TODO: set debug info.
          // Builder.SetCurrentDebugLocation(DL);
        }
        handleMapOnFunction[F] =
            Builder.CreateCall(createHandle, createHandleArgs, handleName);
      }
    }
  }

  for (auto U = GV->user_begin(), E = GV->user_end(); U != E;) {
    User *user = *(U++);
    // Skip unused user.
    if (user->user_empty())
      continue;

    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      Function *userF = CI->getParent()->getParent();
      DXASSERT(handleMapOnFunction.count(userF), "must exist");
      Value *handle = handleMapOnFunction[userF];
      CI->replaceAllUsesWith(handle);
      CI->eraseFromParent();
    } else {
      DXASSERT(dyn_cast<GEPOperator>(user) != nullptr,
               "else AddOpcodeParamForIntrinsic in CodeGen did not patch uses "
               "to only have ld/st refer to temp object");
      GEPOperator *GEP = cast<GEPOperator>(user);
      Value *idx = nullptr;
      if (GEP->getNumIndices() == 2) {
        // one dim array of resource
        idx = (GEP->idx_begin() + 1)->get();
      } else {
        gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);
        // Must be instruction for multi dim array.
        std::unique_ptr<IRBuilder<>> Builder;
        if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP)) {
          Builder = std::make_unique<IRBuilder<>>(GEPInst);
        } else {
          Builder = std::make_unique<IRBuilder<>>(GV->getContext());
        }
        for (; GEPIt != E; ++GEPIt) {
          if (GEPIt->isArrayTy()) {
            unsigned arraySize = GEPIt->getArrayNumElements();
            Value *tmpIdx = GEPIt.getOperand();
            if (idx == nullptr)
              idx = tmpIdx;
            else {
              idx = Builder->CreateMul(idx, Builder->getInt32(arraySize));
              idx = Builder->CreateAdd(idx, tmpIdx);
            }
          }
        }
      }
      createHandleArgs[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] = idx;
      // if (!NonUniformSet.count(idx))
      //  createHandleArgs[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
      //      isUniformRes;
      // else
      //  createHandleArgs[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
      //      hlslOP->GetI1Const(1);

      Value *handle = nullptr;
      if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP)) {
        IRBuilder<> Builder = IRBuilder<>(GEPInst);
        createHandleArgs[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] =
            Builder.CreateAdd(idx, resLowerBound);
        handle = Builder.CreateCall(createHandle, createHandleArgs, handleName);
      }

      for (auto GEPU = GEP->user_begin(), GEPE = GEP->user_end();
           GEPU != GEPE;) {
        // Must be load inst.
        CallInst *CI = cast<CallInst>(*(GEPU++));
        if (!handle) {
          IRBuilder<> Builder = IRBuilder<>(CI);
          createHandleArgs[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] =
              Builder.CreateAdd(idx, resLowerBound);
          Value *localHandle =
              Builder.CreateCall(createHandle, createHandleArgs, handleName);
          CI->replaceAllUsesWith(localHandle);
        } else {
          CI->replaceAllUsesWith(handle);
        }
        CI->eraseFromParent();
      }

      if (Instruction *I = dyn_cast<Instruction>(GEP)) {
        I->eraseFromParent();
      }
    }
  }
  // Erase unused handle.
  for (auto It : handleMapOnFunction) {
    Instruction *I = It.second;
    if (I->user_empty())
      I->eraseFromParent();
  }
}

void DxilLowerCreateHandleForLib::GenerateDxilResourceHandles() {
  // Create sampler handle first, may be used by SRV operations.
  for (size_t i = 0; i < m_DM->GetSamplers().size(); i++) {
    DxilSampler &S = m_DM->GetSampler(i);
    TranslateDxilResourceUses(S);
  }

  for (size_t i = 0; i < m_DM->GetSRVs().size(); i++) {
    DxilResource &SRV = m_DM->GetSRV(i);
    TranslateDxilResourceUses(SRV);
  }

  for (size_t i = 0; i < m_DM->GetUAVs().size(); i++) {
    DxilResource &UAV = m_DM->GetUAV(i);
    TranslateDxilResourceUses(UAV);
  }
}

void DxilLowerCreateHandleForLib::GenerateDxilCBufferHandles() {
  // For CBuffer, handle are mapped to HLCreateHandle.
  OP *hlslOP = m_DM->GetOP();
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(m_DM->GetCtx()));
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);

  Value *resClassArg = hlslOP->GetU8Const(
      static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
          DXIL::ResourceClass::CBuffer));

  for (size_t i = 0; i < m_DM->GetCBuffers().size(); i++) {
    DxilCBuffer &CB = m_DM->GetCBuffer(i);
    GlobalVariable *GV = cast<GlobalVariable>(CB.GetGlobalSymbol());
    // Remove GEP created in HLObjectOperationLowerHelper::UniformCbPtr.
    GV->removeDeadConstantUsers();
    std::string handleName = std::string(GV->getName()) + "_buffer";

    Value *args[] = {opArg, resClassArg, nullptr, nullptr,
                     hlslOP->GetI1Const(0)};
    DIVariable *DIV = nullptr;
    DILocation *DL = nullptr;
    if (m_HasDbgInfo) {
      DebugInfoFinder &Finder = m_DM->GetOrCreateDebugInfoFinder();
      DIV = FindGlobalVariableDebugInfo(GV, Finder);
      if (DIV)
        // TODO: how to get col?
        DL = DILocation::get(createHandle->getContext(), DIV->getLine(), 1,
                             DIV->getScope());
    }

    Value *resIDArg = hlslOP->GetU32Const(CB.GetID());
    args[DXIL::OperandIndex::kCreateHandleResIDOpIdx] = resIDArg;

    // resLowerBound will be added after allocation in DxilCondenseResources.
    Value *resLowerBound = hlslOP->GetU32Const(CB.GetLowerBound());

    if (CB.GetRangeSize() == 1) {
      args[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] = resLowerBound;
      for (auto U = GV->user_begin(); U != GV->user_end();) {
        // Must CreateHandleForLib.
        CallInst *CI = cast<CallInst>(*(U++));
        DxilInst_CreateHandleFromResourceStructForLib createHandleForLib(CI);
        // Put createHandle to entry block.
        auto InsertPt =
            CI->getParent()->getParent()->getEntryBlock().getFirstInsertionPt();
        IRBuilder<> Builder(InsertPt);

        CallInst *handle = Builder.CreateCall(createHandle, args, handleName);
        if (m_HasDbgInfo) {
          // TODO: add debug info.
          // handle->setDebugLoc(DL);
        }
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    } else {
      for (auto U = GV->user_begin(); U != GV->user_end();) {
        // Must GEP
        GEPOperator *GEP = cast<GEPOperator>(*(U++));
        DXASSERT(GEP->getNumIndices() == 2, "else invalid cbv array ptr");
        auto *it = GEP->idx_begin();
        it++;
        Value *CBIndex = *it;
        for (auto GEPU = GEP->user_begin(); GEPU != GEP->user_end();) {
          CallInst *CI = cast<CallInst>(*(GEPU++));
          IRBuilder<> Builder(CI);
          args[DXIL::OperandIndex::kCreateHandleResIndexOpIdx] =
              Builder.CreateAdd(CBIndex, resLowerBound);
          if (isa<ConstantInt>(CBIndex)) {
            // Put createHandle to entry block for const index.
            auto InsertPt = CI->getParent()
                                ->getParent()
                                ->getEntryBlock()
                                .getFirstInsertionPt();
            Builder.SetInsertPoint(InsertPt);
          }
          // if (!NonUniformSet.count(CBIndex))
          //  args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
          //      hlslOP->GetI1Const(0);
          // else
          //  args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
          //      hlslOP->GetI1Const(1);

          CallInst *handle = Builder.CreateCall(createHandle, args, handleName);
          CI->replaceAllUsesWith(handle);
          CI->eraseFromParent();
        }
        if (Instruction *I = dyn_cast<Instruction>(GEP)) {
          I->eraseFromParent();
        }
      }
    }
  }
}

// TBuffer.
namespace {
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

void PatchTBufferLoad(CallInst *handle, DxilModule &DM) {
  hlsl::OP *hlslOP = DM.GetOP();
  llvm::LLVMContext &Ctx = DM.GetCtx();
  Type *doubleTy = Type::getDoubleTy(Ctx);
  Type *i64Ty = Type::getInt64Ty(Ctx);

  // Replace corresponding cbuffer loads with typed buffer loads
  for (auto U = handle->user_begin(); U != handle->user_end();) {
    CallInst *I = cast<CallInst>(*(U++));
    DXASSERT(I && OP::IsDxilOpFuncCallInst(I),
             "otherwise unexpected user of CreateHandle value");
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
      CallInst *load =
          Builder.CreateCall(BufLoad, {opArg, handle, offset, undefI});

      // Find extractelement uses of cbuffer load and replace + generate bitcast
      // as necessary
      for (auto LU = I->user_begin(); LU != I->user_end();) {
        ExtractValueInst *evInst = dyn_cast<ExtractValueInst>(*(LU++));
        DXASSERT(evInst && evInst->getNumIndices() == 1,
                 "user of cbuffer load result should be extractvalue");
        uint64_t idx = evInst->getIndices()[0];
        Type *EltTy = evInst->getType();
        IRBuilder<> EEBuilder(evInst);
        Value *result = nullptr;
        if (EltTy != Ty) {
          // extract two values and DXIL::OpCode::MakeDouble or construct i64
          if ((EltTy == doubleTy) || (EltTy == i64Ty)) {
            DXASSERT(idx < 2, "64-bit component index out of range");

            // This assumes big endian order in tbuffer elements (is this
            // correct?)
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
      DXASSERT(false, "otherwise CBufferLoad used for tbuffer rather than "
                      "CBufferLoadLegacy");
    } else {
      DXASSERT(false, "otherwise unexpected user of CreateHandle value");
    }
    I->eraseFromParent();
  }
}
} // namespace
void DxilLowerCreateHandleForLib::PatchTBufferUse(Value *V, DxilModule &DM) {
  for (User *U : V->users()) {
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      // Patch dxil call.
      if (hlsl::OP::IsDxilOpFuncCallInst(CI))
        PatchTBufferLoad(CI, DM);
    } else {
      PatchTBufferUse(U, DM);
    }
  }
}

void DxilLowerCreateHandleForLib::PatchTBuffers(DxilModule &DM) {
  // move tbuffer resources to SRVs
  unsigned offset = DM.GetSRVs().size();
  Module &M = *DM.GetModule();
  for (auto it = DM.GetCBuffers().begin(); it != DM.GetCBuffers().end(); it++) {
    DxilCBuffer *CB = it->get();
    if (CB->GetKind() == DXIL::ResourceKind::TBuffer) {
      auto srv = make_unique<DxilResource>();
      InitTBuffer(CB, srv.get());
      srv->SetID(offset++);
      DM.AddSRV(std::move(srv));
      GlobalVariable *GV = cast<GlobalVariable>(CB->GetGlobalSymbol());
      PatchTBufferUse(GV, DM);
      // Set global symbol for cbuffer to an unused value so it can be removed
      // in RemoveUnusedResourceSymbols.
      Type *Ty = GV->getType()->getElementType();
      GlobalVariable *NewGV = new GlobalVariable(
          M, Ty, GV->isConstant(), GV->getLinkage(), /*Initializer*/ nullptr,
          GV->getName(),
          /*InsertBefore*/ nullptr, GV->getThreadLocalMode(),
          GV->getType()->getAddressSpace(), GV->isExternallyInitialized());
      CB->SetGlobalSymbol(NewGV);
    }
  }
}

// Select on handle.
// Transform
// A = Add(a0, a1);
// B = Add(b0, b1);
// C = Add(c0, c1);
// Inst = phi A, B, C
//   into
// phi0 = phi a0, b0, c0
// phi1 = phi a1, b1, c1
// NewInst = Add(phi0, phi1);
namespace {
void CollectSelect(llvm::Instruction *Inst,
                   std::unordered_set<llvm::Instruction *> &selectSet) {
  unsigned startOpIdx = 0;
  // Skip Cond for Select.
  if (isa<SelectInst>(Inst)) {
    startOpIdx = 1;
  } else if (!isa<PHINode>(Inst)) {
    // Only check phi and select here.
    return;
  }
  // Already add.
  if (selectSet.count(Inst))
    return;

  selectSet.insert(Inst);

  // Scan operand to add node which is phi/select.
  unsigned numOperands = Inst->getNumOperands();
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *V = Inst->getOperand(i);
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      CollectSelect(I, selectSet);
    }
  }
}

void CreateOperandSelect(Instruction *SelInst, Instruction *Prototype,
                         std::unordered_map<Instruction *, Instruction *>
                             &selInstToSelOperandInstMap) {
  IRBuilder<> Builder(SelInst);

  if (SelectInst *Sel = dyn_cast<SelectInst>(SelInst)) {
    Value *Cond = Sel->getCondition();

    Instruction *newSel = Prototype->clone();
    for (unsigned i = 0; i < Prototype->getNumOperands(); i++) {
      Value *op = Prototype->getOperand(i);
      // Don't replace constant int operand.
      if (isa<UndefValue>(op)) {
        Value *selOperand = Builder.CreateSelect(Cond, op, op);
        newSel->setOperand(i, selOperand);
      }
    }

    Builder.Insert(newSel);

    selInstToSelOperandInstMap[SelInst] = newSel;
    SelInst->replaceAllUsesWith(newSel);
  } else {
    Instruction *newSel = Prototype->clone();
    PHINode *Phi = cast<PHINode>(SelInst);
    unsigned numIncoming = Phi->getNumIncomingValues();

    for (unsigned i = 0; i < Prototype->getNumOperands(); i++) {
      Value *op = Prototype->getOperand(i);
      if (isa<UndefValue>(op)) {
        // Don't replace constant int operand.
        PHINode *phiOp = Builder.CreatePHI(op->getType(), numIncoming);
        for (unsigned j = 0; j < numIncoming; j++) {
          BasicBlock *BB = Phi->getIncomingBlock(j);
          phiOp->addIncoming(op, BB);
        }
        newSel->setOperand(i, phiOp);
      }
    }
    // Insert newSel after phi insts.
    Builder.SetInsertPoint(Phi->getParent()->getFirstNonPHI());
    Builder.Insert(newSel);
    selInstToSelOperandInstMap[SelInst] = newSel;
    SelInst->replaceAllUsesWith(newSel);
  }
}

bool MergeSelectOnSameValue(Instruction *SelInst, unsigned startOpIdx,
                            unsigned numOperands) {
  Value *op0 = nullptr;
  for (unsigned i = startOpIdx; i < numOperands; i++) {
    Value *op = SelInst->getOperand(i);
    if (i == startOpIdx) {
      op0 = op;
    } else {
      if (op0 != op)
        return false;
    }
  }
  if (op0) {
    SelInst->replaceAllUsesWith(op0);
    SelInst->eraseFromParent();
    return true;
  }
  return false;
}

void UpdateOperandSelect(Instruction *SelInst,
                         std::unordered_map<Instruction *, Instruction *>
                             &selInstToSelOperandInstMap,
                         unsigned nonUniformOpIdx,
                         std::unordered_set<Instruction *> &nonUniformOps,
                         std::unordered_set<Instruction *> &invalidSel) {
  unsigned numOperands = SelInst->getNumOperands();

  unsigned startOpIdx = 0;
  // Skip Cond for Select.
  if (SelectInst *Sel = dyn_cast<SelectInst>(SelInst))
    startOpIdx = 1;

  Instruction *newInst = selInstToSelOperandInstMap[SelInst];
  // Transform
  // A = Add(a0, a1);
  // B = Add(b0, b1);
  // C = Add(c0, c1);
  // Inst = phi A, B, C
  //   into
  // phi0 = phi a0, b0, c0
  // phi1 = phi a1, b1, c1
  // NewInst = Add(phi0, phi1);
  for (unsigned i = 0; i < newInst->getNumOperands(); i++) {
    Value *op = newInst->getOperand(i);
    // Skip not select operand.
    if (!isa<SelectInst>(op) && !isa<PHINode>(op))
      continue;
    Instruction *opI = cast<Instruction>(op);
    // Each operand of newInst is a select inst.
    // Now we set phi0 operands based on operands of phi A, B, C.
    for (unsigned j = startOpIdx; j < numOperands; j++) {
      Instruction *selOp = dyn_cast<Instruction>(SelInst->getOperand(j));
      if (!selOp) {
        // Fail to map selOp to prototype inst at SelInst.
        invalidSel.insert(SelInst);
        continue;
      }

      auto it = selInstToSelOperandInstMap.find(selOp);
      if (it != selInstToSelOperandInstMap.end()) {
        // Map the new created inst.
        selOp = it->second;
      } else {
        // Make sure selOp match newInst format.
        if (selOp->getOpcode() != newInst->getOpcode()) {
          // Fail to map selOp to prototype inst at SelInst.
          invalidSel.insert(SelInst);
          continue;
        }
        // Make sure function is the same.
        if (isa<CallInst>(selOp) && isa<CallInst>(newInst)) {
          if (cast<CallInst>(selOp)->getCalledFunction() !=
              cast<CallInst>(newInst)->getCalledFunction()) {
            // Fail to map selOp to prototype inst at SelInst.
            invalidSel.insert(SelInst);
            continue;
          }
        }
      }
      // Here we set phi0 operand j with operand i of jth operand from (phi A,
      // B, C).
      opI->setOperand(j, selOp->getOperand(i));
    }
    // Remove select if all operand is the same.
    if (!MergeSelectOnSameValue(opI, startOpIdx, numOperands) &&
        i != nonUniformOpIdx) {
      // Save nonUniform for later check.
      nonUniformOps.insert(opI);
    }
  }
}

} // namespace

void DxilLowerCreateHandleForLib::AddCreateHandleForPhiNodeAndSelect(
    OP *hlslOP) {
  Function *createHandle = hlslOP->GetOpFunc(
      OP::OpCode::CreateHandle, llvm::Type::getVoidTy(hlslOP->GetCtx()));

  std::unordered_set<PHINode *> objPhiList;
  std::unordered_set<SelectInst *> objSelectList;
  std::unordered_set<Instruction *> resSelectSet;
  for (User *U : createHandle->users()) {
    for (User *HandleU : U->users()) {
      Instruction *I = cast<Instruction>(HandleU);
      if (!isa<CallInst>(I))
        CollectSelect(I, resSelectSet);
    }
  }

  // Generate Handle inst for Res inst.
  FunctionType *FT = createHandle->getFunctionType();
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandle);
  Type *resClassTy =
      FT->getParamType(DXIL::OperandIndex::kCreateHandleResClassOpIdx);
  Type *resIDTy = FT->getParamType(DXIL::OperandIndex::kCreateHandleResIDOpIdx);
  Type *resAddrTy =
      FT->getParamType(DXIL::OperandIndex::kCreateHandleResIndexOpIdx);
  Value *UndefResClass = UndefValue::get(resClassTy);
  Value *UndefResID = UndefValue::get(resIDTy);
  Value *UndefResAddr = UndefValue::get(resAddrTy);

  // phi/select node resource is not uniform
  Value *nonUniformRes = hlslOP->GetI1Const(1);

  std::unique_ptr<CallInst> PrototypeCall(
      CallInst::Create(createHandle, {opArg, UndefResClass, UndefResID,
                                      UndefResAddr, nonUniformRes}));

  std::unordered_map<Instruction *, Instruction *> handleMap;
  for (Instruction *SelInst : resSelectSet) {
    CreateOperandSelect(SelInst, PrototypeCall.get(), handleMap);
  }

  // Update operand for Handle phi/select.
  // If ResClass or ResID is phi/select, save to nonUniformOps.
  std::unordered_set<Instruction *> nonUniformOps;
  std::unordered_set<Instruction *> invalidSel;
  for (Instruction *SelInst : resSelectSet) {
    UpdateOperandSelect(SelInst, handleMap,
                        // Index into range is ok to diverse.
                        DxilInst_CreateHandle::arg_index, nonUniformOps,
                        invalidSel);
  }

  if (!invalidSel.empty()) {
    for (Instruction *I : invalidSel) {
      // Non uniform res class or res id.
      dxilutil::EmitResMappingError(I);
    }
    return;
  }

  // ResClass and ResID must be uniform.
  // Try to merge res class, res id into imm recursive.
  while (1) {
    bool bUpdated = false;

    for (auto It = nonUniformOps.begin(); It != nonUniformOps.end();) {
      Instruction *I = *(It++);
      unsigned numOperands = I->getNumOperands();

      unsigned startOpIdx = 0;
      // Skip Cond for Select.
      if (SelectInst *Sel = dyn_cast<SelectInst>(I))
        startOpIdx = 1;
      if (MergeSelectOnSameValue(I, startOpIdx, numOperands)) {
        nonUniformOps.erase(I);
        bUpdated = true;
      }
    }

    if (!bUpdated) {
      if (!nonUniformOps.empty()) {
        for (Instruction *I : nonUniformOps) {
          // Non uniform res class or res id.
          dxilutil::EmitResMappingError(I);
        }
        return;
      }
      break;
    }
  }

  // Remove useless select/phi.
  for (Instruction *Res : resSelectSet) {
    Res->eraseFromParent();
  }
}
} // namespace

char DxilLowerCreateHandleForLib::ID = 0;

ModulePass *llvm::createDxilLowerCreateHandleForLibPass() {
  return new DxilLowerCreateHandleForLib();
}

INITIALIZE_PASS(DxilLowerCreateHandleForLib, "hlsl-dxil-lower-handle-for-lib", "DXIL Lower createHandleForLib", false, false)
