///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilTrimCBufferMembersPass.cpp                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// HLSL Change - Opt-in pass (run with -opt-enable dxil-trim-cbuffer) that   //
// removes unused members from cbuffers and compacts the layout: dead        //
// members are dropped, surviving member offsets are re-packed, and load     //
// instructions (CBufferLoad / CBufferLoadLegacy) are rewritten to the new   //
// layout. The cbuffer size in dx.resources and the struct annotation in     //
// dx.typeAnnotations are updated accordingly.                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

using namespace llvm;
using namespace hlsl;

namespace {
// HLSL Change Starts - Trim unused cbuffer members.
//
// Removes unused members from cbuffer structs and compacts the layout:
// dead members are dropped, surviving member offsets are re-packed, and
// load instructions (CBufferLoad / CBufferLoadLegacy) are rewritten to
// the new layout. The cbuffer size in dx.resources and the struct
// annotation in dx.typeAnnotations are updated accordingly, so CPU-side
// reflection consumers see the compacted layout.
//
// Granularity rules:
// - CBufferLoad (opcode 58): byte granular. byteOffset operands are
//   remapped to the new member offsets. Cbuffers accessed only this way
//   get full byte-level compaction.
// - CBufferLoadLegacy (opcode 59): 16-byte register granular. A register
//   is kept as a monolith (intra-register layout must not change because
//   the ExtractValue lane picking depends on it), registers are only
//   renumbered. Drops fully-unused trailing/interior registers.
// - Any cbuffer whose handle has an unresolvable origin, a non-constant
//   offset/regIndex, or any non-load use is skipped (conservative).
class DxilTrimCBufferMembers : public ModulePass {
public:
  static char ID; // pass identification, replacement for typeid
  explicit DxilTrimCBufferMembers() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "Trim unused cbuffer members";
  }

  bool runOnModule(Module &M) override;

private:
  struct CBufferUsage {
    DxilCBuffer *CB = nullptr;
    DxilStructAnnotation *SA = nullptr;
    llvm::StructType *ST = nullptr;
    // Wrapper structs above the member struct (ConstantBuffer<T> lowers
    // to { T }): recorded outermost-first so trimCBuffer can rebuild them.
    SmallVector<std::pair<llvm::StructType *, DxilStructAnnotation *>, 2>
        Wrappers;
    bool Skip = false;          // keep everything, do not touch
    bool LegacyRegisterMode = false; // any CBufferLoadLegacy seen
    std::set<unsigned> UsedRegisters;   // legacy mode: used reg indexes
    std::vector<bool> UsedBytes;        // byte mode: per-byte usage map
    std::vector<CallInst *> Loads;      // all loads on this cbuffer
  };

  bool collectUsage(Module &M, DxilModule &DM,
                    std::vector<std::unique_ptr<CBufferUsage>> &Usages);
  bool trimCBuffer(DxilModule &DM, CBufferUsage &Usage);
  static unsigned typeAlignmentForCBuffer(Type *Ty);
  static unsigned loadSizeForReturnType(Type *RetTy);
  static unsigned realMemberSize(DxilTypeSystem &TS, bool bMinPrec, Type *Ty,
                                 const DxilFieldAnnotation &FA);
};

char DxilTrimCBufferMembers::ID = 0;

// Size in bytes of the value returned by a CBufferLoad overload. The
// lowering generates one load per scalar element (HLOperationLower.cpp
// GenerateCBLoad), so only scalar types occur in practice.
unsigned DxilTrimCBufferMembers::loadSizeForReturnType(Type *RetTy) {
  if (RetTy->isSingleValueType() && !RetTy->isVectorTy()) {
    if (RetTy->isDoubleTy())
      return 8;
    if (RetTy->isHalfTy() || RetTy->isIntegerTy(16))
      return 2;
    return 4; // i32 / f32
  }
  return 0; // unexpected aggregate, caller treats as bail-out
}

// Data extent in bytes of a cbuffer member, following the same packing
// formula as the reflection (DxilContainerReflection.cpp:1423). This is
// the real data size without tail padding, unlike the gap-to-next-member
// diff used for owner partitioning.
unsigned DxilTrimCBufferMembers::realMemberSize(DxilTypeSystem &TS,
                                                bool bMinPrec, Type *Ty,
                                                const DxilFieldAnnotation &FA) {
  unsigned compSize = 4;
  switch (FA.GetCompType().GetKind()) {
  case DXIL::ComponentType::F64:
  case DXIL::ComponentType::SNormF64:
  case DXIL::ComponentType::UNormF64:
  case DXIL::ComponentType::I64:
  case DXIL::ComponentType::U64:
    compSize = 8;
    break;
  case DXIL::ComponentType::I16:
  case DXIL::ComponentType::U16:
  case DXIL::ComponentType::F16:
  case DXIL::ComponentType::SNormF16:
  case DXIL::ComponentType::UNormF16:
    compSize = bMinPrec ? 4 : 2;
    break;
  default:
    compSize = 4;
    break;
  }

  unsigned cbRows = 1, cbCols = 1;
  if (FA.HasMatrixAnnotation()) {
    const DxilMatrixAnnotation &MA = FA.GetMatrixAnnotation();
    cbRows = MA.Rows;
    cbCols = MA.Cols;
    if (MA.Orientation != MatrixOrientation::RowMajor)
      std::swap(cbRows, cbCols);
  } else if (Ty->isVectorTy()) {
    cbRows = 1;
    cbCols = Ty->getVectorNumElements();
  }

  // Array dimension (array of scalars/vectors, or array of matrices).
  unsigned elements = 1;
  if (FA.HasMatrixAnnotation()) {
    // Matrices are lowered to [N x [R x C]] arrays; only count outer
    // dimensions beyond the matrix shape itself.
    while (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
      Type *Elt = AT->getArrayElementType();
      if (Elt->isVectorTy())
        break; // this is the matrix body
      elements *= AT->getNumElements();
      Ty = Elt;
    }
  } else if (Ty->isArrayTy()) {
    while (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
      elements *= AT->getNumElements();
      Ty = AT->getArrayElementType();
    }
    if (!Ty->isVectorTy() && !Ty->isSingleValueType()) {
      // Array of structs: each element 16-aligned, size from nested
      // annotation.
      if (StructType *NestedST = dyn_cast<StructType>(Ty)) {
        if (DxilStructAnnotation *NestedSA = TS.GetStructAnnotation(NestedST)) {
          unsigned elt = NestedSA->GetCBufferSize();
          if (elt == 0)
            return 0;
          unsigned stride = (elt + 15) & ~15u;
          return stride * (elements - 1) + elt;
        }
      }
      return 0; // unknown, caller falls back to diff size
    }
  } else if (StructType *NestedST = dyn_cast<StructType>(Ty)) {
    if (DxilStructAnnotation *NestedSA = TS.GetStructAnnotation(NestedST))
      return NestedSA->GetCBufferSize();
    return 0;
  }

  if (elements > 1)
    cbRows = cbRows * elements;
  unsigned rowStride = 16;
  if (compSize > 4 && cbCols > 2)
    rowStride = 32;
  return rowStride * (cbRows - 1) + compSize * cbCols;
}

// Alignment used when re-packing a surviving member. Scalar/vector
// members keep their natural alignment, everything else (arrays,
// matrices, nested structs) aligns to 16 as per HLSL packing rules.
unsigned DxilTrimCBufferMembers::typeAlignmentForCBuffer(Type *Ty) {
  if (Ty->isVectorTy()) {
    unsigned n = Ty->getVectorNumElements();
    if (Ty->getScalarSizeInBits() == 16)
      return n == 1 ? 2 : (n == 2 ? 4 : 8);
    if (n == 2)
      return 8;
    return 16; // vec3/vec4 never straddle a 16-byte register
  }
  if (Ty->isFloatTy() || Ty->isIntegerTy(32))
    return 4;
  if (Ty->isDoubleTy())
    return 8;
  if (Ty->isHalfTy() || Ty->isIntegerTy(16))
    return 2;
  return 16; // array (incl. matrix), struct, opaque fallback
}

bool DxilTrimCBufferMembers::collectUsage(
    Module &M, DxilModule &DM,
    std::vector<std::unique_ptr<CBufferUsage>> &Usages) {
  // Map each cbuffer to its usage record.
  std::map<std::pair<DxilResourceBase *, unsigned>, CBufferUsage *> byRes;
  DenseMap<Value *, CBufferUsage *> handleMap;

  auto getUsage = [&](DxilCBuffer *CB) -> CBufferUsage * {
    auto key = std::make_pair(static_cast<DxilResourceBase *>(CB), 0u);
    auto it = byRes.find(key);
    if (it != byRes.end())
      return it->second;
    // HLSLType is not serialized for pre-SM6.6 modules; recover it from
    // the global symbol's pointer type (%struct.MYCB* undef).
    Type *HLSLType = CB->GetHLSLType();
    if (HLSLType == nullptr) {
      if (Constant *GS = CB->GetGlobalSymbol())
        HLSLType = GS->getType();
    }
    if (HLSLType == nullptr || !HLSLType->isPointerTy()) {
      return nullptr;
    }
    Type *ElemTy = HLSLType->getPointerElementType();
    StructType *ST = dyn_cast<StructType>(ElemTy);
    if (ST == nullptr || ST->isOpaque() || ElemTy->isArrayTy()) {
      return nullptr;
    }
    DxilStructAnnotation *SA = DM.GetTypeSystem().GetStructAnnotation(ST);
    if (SA == nullptr || SA->GetNumFields() != ST->getNumElements() ||
        SA->GetNumTemplateArgs() != 0 || ST->getNumElements() == 0) {
      return nullptr;
    }
    auto Usage = llvm::make_unique<CBufferUsage>();
    Usage->CB = CB;
    Usage->SA = SA;
    Usage->ST = ST;
    // Drill through single-field wrapper structs (ConstantBuffer<T>
    // lowers to { T }): the nested fields live at absolute offsets and
    // are the real trim candidates.
    while (ST->getNumElements() == 1) {
      Type *FT = ST->getElementType(0);
      StructType *NestedST = dyn_cast<StructType>(FT);
      if (NestedST == nullptr || NestedST->isOpaque())
        break;
      DxilStructAnnotation *NestedSA =
          DM.GetTypeSystem().GetStructAnnotation(NestedST);
      if (NestedSA == nullptr ||
          NestedSA->GetNumFields() != NestedST->getNumElements() ||
          NestedSA->GetNumTemplateArgs() != 0 ||
          NestedSA->GetNumFields() == 0 ||
          !NestedSA->GetFieldAnnotation(0).HasCBufferOffset())
        break;
      Usage->Wrappers.push_back({ST, SA});
      ST = NestedST;
      SA = NestedSA;
    }
    Usage->SA = SA;
    Usage->ST = ST;
    Usage->UsedBytes.resize(SA->GetCBufferSize(), false);
    CBufferUsage *Ptr = Usage.get();
    byRes[key] = Ptr;
    Usages.push_back(std::move(Usage));
    return Ptr;
  };

  // Resolve a handle value to a usage record, or null when unknown.
  std::function<CBufferUsage *(Value *)> resolve =
      [&](Value *V) -> CBufferUsage * {
        auto it = handleMap.find(V);
        if (it != handleMap.end())
          return it->second;
        CBufferUsage *Found = nullptr;
        if (CallInst *CI = dyn_cast<CallInst>(V)) {
          DxilInst_CreateHandle createHandle(CI);
          DxilInst_CreateHandleForLib createHandleForLib(CI);
          DxilInst_AnnotateHandle annotateHandle(CI);
          if (createHandle) {
            if (createHandle.get_resourceClass_val() ==
                static_cast<int8_t>(DXIL::ResourceClass::CBuffer)) {
              unsigned rangeId = static_cast<unsigned>(
                  createHandle.get_rangeId_val());
              if (rangeId < DM.GetCBuffers().size())
                Found = getUsage(&DM.GetCBuffer(rangeId));
            }
          } else if (createHandleForLib) {
            // Resolve the resource global through the symbol table.
            Value *Res = createHandleForLib.get_Resource();
            if (LoadInst *LI = dyn_cast<LoadInst>(Res)) {
              Value *Ptr = LI->getPointerOperand();
              if (GEPOperator *GEP = dyn_cast<GEPOperator>(Ptr))
                Ptr = GEP->getPointerOperand();
              for (auto &CB : DM.GetCBuffers()) {
                if (CB->GetGlobalSymbol() == Ptr) {
                  Found = getUsage(CB.get());
                  break;
                }
              }
            } else {
              for (auto &CB : DM.GetCBuffers()) {
                if (CB->GetGlobalSymbol() == Res) {
                  Found = getUsage(CB.get());
                  break;
                }
              }
            }
          } else if (DxilInst_CreateHandleFromBinding createHandleFromBinding{CI}) {
            // HLSL Change - SM6.6+ creates cbuffer handles via
            // dx.op.createHandleFromBinding. %dx.types.ResBind is
            // { i32 rangeLowerBound, i32 rangeUpperBound, i32 spaceID, i8 resClass }.
            Constant *Bind = dyn_cast<Constant>(createHandleFromBinding.get_bind());
            if (Bind != nullptr) {
              auto LowerCI = dyn_cast_or_null<ConstantInt>(Bind->getAggregateElement(0u));
              auto SpaceCI = dyn_cast_or_null<ConstantInt>(Bind->getAggregateElement(2u));
              auto ClassCI = dyn_cast_or_null<ConstantInt>(Bind->getAggregateElement(3u));
              if (LowerCI && SpaceCI && ClassCI &&
                  ClassCI->getSExtValue() ==
                      static_cast<int8_t>(DXIL::ResourceClass::CBuffer)) {
                unsigned lowerBound = static_cast<unsigned>(LowerCI->getZExtValue());
                unsigned spaceID = static_cast<unsigned>(SpaceCI->getZExtValue());
                for (auto &CB : DM.GetCBuffers()) {
                  if (CB->GetSpaceID() == spaceID &&
                      lowerBound == CB->GetLowerBound()) {
                    Found = getUsage(CB.get());
                    break;
                  }
                }
              }
            }
          } else if (annotateHandle) {
            Found = resolve(annotateHandle.get_res());
          }
        }
        if (Found == nullptr)
          Found = (CBufferUsage *)~0ull; // poison: unresolved
        handleMap[V] = Found;
        return Found;
      };

  bool AnyCandidate = false;
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      CallInst *CI = dyn_cast<CallInst>(&*I);
      if (CI == nullptr)
        continue;
      DxilInst_CBufferLoad cbLoad(CI);
      DxilInst_CBufferLoadLegacy cbLoadLegacy(CI);
      DxilInst_AnnotateHandle annotateHandle(CI);
      if (cbLoad || cbLoadLegacy) {
        CBufferUsage *Usage = resolve(CI->getOperand(1));
        if (Usage == nullptr)
          continue; // load on non-cbuffer or non-trimmable cbuffer
        if (Usage == (CBufferUsage *)~0ull) {
          continue; // unresolved origin, leave alone
        }
        Usage->Loads.push_back(CI);
        if (cbLoadLegacy) {
          Usage->LegacyRegisterMode = true;
          if (ConstantInt *RegIdx =
                  dyn_cast<ConstantInt>(cbLoadLegacy.get_regIndex())) {
            Usage->UsedRegisters.insert(
                static_cast<unsigned>(RegIdx->getZExtValue()));
          } else {
            Usage->Skip = true; // dynamic register index
          }
        } else {
          if (ConstantInt *Off =
                  dyn_cast<ConstantInt>(cbLoad.get_byteOffset())) {
            unsigned offset = static_cast<unsigned>(Off->getZExtValue());
            unsigned size = loadSizeForReturnType(CI->getType());
            if (size == 0 || offset > Usage->UsedBytes.size() ||
                size > Usage->UsedBytes.size() - offset) {
              Usage->Skip = true; // out-of-range load, be safe
            } else {
              for (unsigned i = 0; i < size; ++i)
                Usage->UsedBytes[offset + i] = true;
            }
          } else {
            Usage->Skip = true; // dynamic byte offset
          }
        }
      } else if (annotateHandle) {
        // Track annotateHandle chains that feed cbuffer loads so resolve()
        // can walk through them; no direct action needed here.
        (void)resolve(CI);
      }
    }
  }

  // Mark cbuffers whose handle escapes into anything but the supported
  // loads as skip. Walk users of every resolved handle creation.
  for (auto &Pair : handleMap) {
    CBufferUsage *Usage = Pair.second;
    if (Usage == nullptr || Usage == (CBufferUsage *)~0ull)
      continue;
    Value *V = Pair.first;
    for (User *U : V->users()) {
      if (CallInst *CI = dyn_cast<CallInst>(U)) {
        DxilInst_CBufferLoad cbLoad(CI);
        DxilInst_CBufferLoadLegacy cbLoadLegacy(CI);
        DxilInst_AnnotateHandle annotateHandle(CI);
        if (cbLoad || cbLoadLegacy || annotateHandle)
          continue;
      }
      Usage->Skip = true; // any other consumer (store, phi, call...)
    }
  }

  for (auto &Usage : Usages) {
    if (Usage->CB != nullptr && !Usage->Skip)
      AnyCandidate = true;
  }
  return AnyCandidate;
}

bool DxilTrimCBufferMembers::trimCBuffer(DxilModule &DM,
                                         CBufferUsage &Usage) {
  DxilCBuffer *CB = Usage.CB;
  DxilStructAnnotation *SA = Usage.SA;
  StructType *ST = Usage.ST;
  unsigned numFields = ST->getNumElements();
  unsigned origSize = SA->GetCBufferSize();

  // Byte ranges of each member from the original annotation. Two sizes
  // are tracked: diffSize (gap to next member, includes tail padding,
  // used to partition bytes for load-owner lookup) and realSize (actual
  // data extent following the reflection packing formula, used for
  // register spans and re-packing).
  struct MemberLayout {
    unsigned Offset;
    unsigned Size;      // partition size (to next member / cbuffer end)
    unsigned RealSize;  // data extent without tail padding
  };
  SmallVector<MemberLayout, 8> members(numFields);
  for (unsigned i = 0; i < numFields; ++i) {
    const DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
    unsigned offset = FA.HasCBufferOffset() ? FA.GetCBufferOffset() : 0;
    unsigned nextOffset =
        (i + 1 < numFields)
            ? (SA->GetFieldAnnotation(i + 1).HasCBufferOffset()
                   ? SA->GetFieldAnnotation(i + 1).GetCBufferOffset()
                   : offset)
            : origSize;
    if (nextOffset < offset)
      nextOffset = offset;
    unsigned real = realMemberSize(DM.GetTypeSystem(),
                                   DM.GetUseMinPrecision(),
                                   ST->getElementType(i), FA);
    if (real == 0 || real > nextOffset - offset)
      real = nextOffset - offset; // fall back to partition size
    members[i] = {offset, nextOffset - offset, real};
  }

  // Decide which members survive.
  SmallVector<bool, 8> keep(numFields, false);
  for (unsigned i = 0; i < numFields; ++i) {
    if (Usage.LegacyRegisterMode) {
      unsigned beginReg = members[i].Offset / 16;
      unsigned endReg =
          (members[i].Offset + members[i].RealSize + 15) / 16;
      for (unsigned r = beginReg; r < endReg; ++r) {
        if (Usage.UsedRegisters.count(r)) {
          keep[i] = true;
          break;
        }
      }
    } else {
      for (unsigned b = members[i].Offset;
           b < members[i].Offset + members[i].Size; ++b) {
        if (b < Usage.UsedBytes.size() && Usage.UsedBytes[b]) {
          keep[i] = true;
          break;
        }
      }
    }
  }

  bool anyKept = false;
  for (unsigned i = 0; i < numFields; ++i)
    anyKept |= keep[i];
  if (!anyKept || (Usage.LegacyRegisterMode && Usage.UsedRegisters.empty()))
    return false; // never shrink to zero; keep original

  // ---- Legacy mode: renumber registers, keep intra-register layout ----
  if (Usage.LegacyRegisterMode) {
    unsigned maxReg = (origSize + 15) / 16;
    SmallVector<int, 16> regMap(maxReg, -1);
    unsigned nextReg = 0;
    for (unsigned r = 0; r < maxReg; ++r) {
      // A register survives if any member intersecting it is kept.
      bool regUsed = false;
      for (unsigned i = 0; i < numFields; ++i) {
        if (!keep[i])
          continue;
        unsigned beginReg = members[i].Offset / 16;
        unsigned endReg =
            (members[i].Offset + members[i].RealSize + 15) / 16;
        if (r >= beginReg && r < endReg) {
          regUsed = true;
          break;
        }
      }
      if (Usage.UsedRegisters.count(r) && !regUsed) {
        // Register is loaded but no member claims it (padding region or
        // fully dead members inside): keep it mapped to preserve sizes.
        regUsed = true;
      }
      if (regUsed)
        regMap[r] = nextReg++;
    }
    if (nextReg == maxReg)
      return false; // no register saved

    for (CallInst *CI : Usage.Loads) {
      DxilInst_CBufferLoadLegacy cbLoadLegacy(CI);
      if (!cbLoadLegacy)
        continue;
      ConstantInt *RegIdx =
          dyn_cast<ConstantInt>(cbLoadLegacy.get_regIndex());
      if (RegIdx == nullptr)
        continue;
      unsigned oldReg = static_cast<unsigned>(RegIdx->getZExtValue());
      if (oldReg >= regMap.size() || regMap[oldReg] < 0)
        continue;
      CI->setOperand(
          2, llvm::ConstantInt::get(Type::getInt32Ty(CI->getContext()),
                                    static_cast<uint64_t>(regMap[oldReg])));
    }

    // Byte-offset loads on a register-mode cbuffer: remap the byteOffset
    // by the owning member's register delta (mixed load kinds).
    for (CallInst *CI : Usage.Loads) {
      DxilInst_CBufferLoad cbLoad(CI);
      if (!cbLoad)
        continue;
      ConstantInt *Off = dyn_cast<ConstantInt>(cbLoad.get_byteOffset());
      if (Off == nullptr)
        continue;
      unsigned offset = static_cast<unsigned>(Off->getZExtValue());
      int owner = -1;
      for (unsigned i = 0; i < numFields; ++i) {
        if (offset >= members[i].Offset &&
            offset < members[i].Offset + members[i].Size) {
          owner = i;
          break;
        }
      }
      if (owner < 0)
        continue;
      unsigned oldOff = members[owner].Offset;
      int newBase = regMap[oldOff / 16];
      if (newBase < 0)
        continue;
      int delta = static_cast<unsigned>(newBase) * 16 +
                      (oldOff & 0xF) -
                      oldOff;
      if (delta == 0)
        continue;
      CI->setOperand(
          2, llvm::ConstantInt::get(
                 Type::getInt32Ty(CI->getContext()),
                 static_cast<uint64_t>(static_cast<int>(offset) + delta)));
    }

    // Rebuild struct + annotation with kept members at their original
    // intra-cbuffer offsets (compacted by register). Members dropped from
    // a partially-kept register cannot exist because register keep
    // implies member keep for every member intersecting it.
    SmallVector<Type *, 8> keptTys;
    SmallVector<unsigned, 8> keptIdx;
    for (unsigned i = 0; i < numFields; ++i) {
      if (!keep[i])
        continue;
      keptTys.push_back(ST->getElementType(i));
      keptIdx.push_back(i);
    }
    StructType *NewST =
        StructType::create(keptTys, ST->getName(), ST->isPacked());
    DxilStructAnnotation *NewSA = DM.GetTypeSystem().AddStructAnnotation(NewST);
    unsigned dst = 0;
    unsigned newSize = 0;
    for (unsigned idx : keptIdx) {
      NewSA->GetFieldAnnotation(dst) = SA->GetFieldAnnotation(idx);
      unsigned oldOff = members[idx].Offset;
      int newBase = regMap[oldOff / 16];
      DXASSERT(newBase >= 0, "kept member must live in kept register");
      unsigned newOff =
          static_cast<unsigned>(newBase) * 16 + (oldOff & 0xF);
      NewSA->GetFieldAnnotation(dst).SetCBufferOffset(newOff);
      newSize = std::max(newSize, newOff + members[idx].RealSize);
      ++dst;
    }
    newSize = (newSize + 15) & ~15u;
    NewSA->SetCBufferSize(newSize);
    DM.GetTypeSystem().FinishStructAnnotation(*NewSA);

    // Re-wrap through ConstantBuffer<T> style wrappers.
    Type *ResultTy = NewST;
    for (auto it = Usage.Wrappers.rbegin(); it != Usage.Wrappers.rend();
         ++it) {
      SmallVector<Type *, 1> WrapTy{ResultTy};
      StructType *NewWrap = StructType::create(
          WrapTy, it->first->getName(), it->first->isPacked());
      DxilStructAnnotation *NewWrapSA =
          DM.GetTypeSystem().AddStructAnnotation(NewWrap);
      NewWrapSA->GetFieldAnnotation(0) = it->second->GetFieldAnnotation(0);
      NewWrapSA->GetFieldAnnotation(0).SetCBufferOffset(0);
      NewWrapSA->SetCBufferSize(newSize);
      DM.GetTypeSystem().FinishStructAnnotation(*NewWrapSA);
      DM.GetTypeSystem().EraseStructAnnotation(it->first);
      ResultTy = NewWrap;
    }

    Type *OrigPtrTy = CB->GetHLSLType();
    if (OrigPtrTy == nullptr)
      OrigPtrTy = CB->GetGlobalSymbol()->getType();
    PointerType *NewPtrTy = PointerType::get(
        ResultTy, OrigPtrTy->getPointerAddressSpace());
    CB->SetHLSLType(NewPtrTy);
    CB->SetGlobalSymbol(UndefValue::get(NewPtrTy));
    CB->SetSize(newSize);
    DM.GetTypeSystem().EraseStructAnnotation(ST);
    return true;
  }

  // ---- Byte mode: re-pack survivors in order, rewrite byteOffsets ----
  SmallVector<unsigned, 8> newOffset(numFields, 0);
  unsigned cursor = 0;
  bool anyMoved = false;
  for (unsigned i = 0; i < numFields; ++i) {
    if (!keep[i])
      continue;
    unsigned align = typeAlignmentForCBuffer(ST->getElementType(i));
    if (align == 0)
      align = 4;
    unsigned aligned = (cursor + align - 1) & ~(align - 1);
    newOffset[i] = aligned;
    anyMoved |= (aligned != members[i].Offset);
    cursor = aligned + members[i].RealSize;
  }
  unsigned newSize = (cursor + 15) & ~15u;
  if (!anyMoved && newSize >= origSize)
    return false; // nothing gained

  for (CallInst *CI : Usage.Loads) {
    DxilInst_CBufferLoad cbLoad(CI);
    if (!cbLoad)
      continue;
    ConstantInt *Off = dyn_cast<ConstantInt>(cbLoad.get_byteOffset());
    if (Off == nullptr)
      continue;
    unsigned offset = static_cast<unsigned>(Off->getZExtValue());
    // Find the member containing this offset.
    int owner = -1;
    for (unsigned i = 0; i < numFields; ++i) {
      if (offset >= members[i].Offset &&
          offset < members[i].Offset + members[i].Size) {
        owner = i;
        break;
      }
    }
    if (owner < 0)
      continue; // load from padding, should not happen for valid DXIL
    int delta = static_cast<int>(newOffset[owner]) -
                static_cast<int>(members[owner].Offset);
    if (delta == 0)
      continue;
    CI->setOperand(
        2, llvm::ConstantInt::get(Type::getInt32Ty(CI->getContext()),
                                  static_cast<uint64_t>(
                                      static_cast<int>(offset) + delta)));
  }

  // Rebuild struct type + annotation with kept members.
  SmallVector<Type *, 8> keptTys;
  SmallVector<unsigned, 8> keptIdx;
  for (unsigned i = 0; i < numFields; ++i) {
    if (!keep[i])
      continue;
    keptTys.push_back(ST->getElementType(i));
    keptIdx.push_back(i);
  }
  StructType *NewST =
      StructType::create(keptTys, ST->getName(), ST->isPacked());
  DxilStructAnnotation *NewSA = DM.GetTypeSystem().AddStructAnnotation(NewST);
  unsigned dst = 0;
  for (unsigned idx : keptIdx) {
    NewSA->GetFieldAnnotation(dst) = SA->GetFieldAnnotation(idx);
    NewSA->GetFieldAnnotation(dst).SetCBufferOffset(newOffset[idx]);
    ++dst;
  }
  NewSA->SetCBufferSize(newSize);
  DM.GetTypeSystem().FinishStructAnnotation(*NewSA);

  // Re-wrap through ConstantBuffer<T> style wrappers.
  Type *ResultTy = NewST;
  for (auto it = Usage.Wrappers.rbegin(); it != Usage.Wrappers.rend(); ++it) {
    SmallVector<Type *, 1> WrapTy{ResultTy};
    StructType *NewWrap =
        StructType::create(WrapTy, it->first->getName(), it->first->isPacked());
    DxilStructAnnotation *NewWrapSA =
        DM.GetTypeSystem().AddStructAnnotation(NewWrap);
    NewWrapSA->GetFieldAnnotation(0) = it->second->GetFieldAnnotation(0);
    NewWrapSA->GetFieldAnnotation(0).SetCBufferOffset(0);
    NewWrapSA->SetCBufferSize(newSize);
    DM.GetTypeSystem().FinishStructAnnotation(*NewWrapSA);
    DM.GetTypeSystem().EraseStructAnnotation(it->first);
    ResultTy = NewWrap;
  }

  Type *OrigPtrTy = CB->GetHLSLType();
  if (OrigPtrTy == nullptr)
    OrigPtrTy = CB->GetGlobalSymbol()->getType();
  PointerType *NewPtrTy =
      PointerType::get(ResultTy, OrigPtrTy->getPointerAddressSpace());
  CB->SetHLSLType(NewPtrTy);
  CB->SetGlobalSymbol(UndefValue::get(NewPtrTy));
  CB->SetSize(newSize);
  DM.GetTypeSystem().EraseStructAnnotation(ST);
  return true;
}

bool DxilTrimCBufferMembers::runOnModule(Module &M) {
  // GetOrCreate (rather than Has+Get) so raw bitcode input also works:
  // dxopt only auto-loads the DxilModule for full container inputs.
  DxilModule &DM = M.GetOrCreateDxilModule();
  if (DM.GetShaderModel() == nullptr)
    return false; // not a DXIL module
  if (DM.GetShaderModel()->IsLib())
    return false; // member usage deferred to link time

  std::vector<std::unique_ptr<CBufferUsage>> Usages;
  if (!collectUsage(M, DM, Usages)) {
    return false;
  }

  bool bChanged = false;
  for (auto &Usage : Usages) {
    if (Usage->CB == nullptr || Usage->Skip || Usage->Loads.empty()) {
      continue;
    }
    // No per-member annotation info means no trimming information.
    if (Usage->SA->GetNumFields() == 0)
      continue;
    bChanged |= trimCBuffer(DM, *Usage);
  }
  if (bChanged) {
    DM.ReEmitDxilResources();
  }
  return bChanged;
}
// HLSL Change Ends

} // namespace

ModulePass *llvm::createDxilTrimCBufferMembersPass() {
  return new DxilTrimCBufferMembers();
}

INITIALIZE_PASS(DxilTrimCBufferMembers, "dxil-trim-cbuffer",
                "Trim unused cbuffer members", false, false)
