//===- ScalarReplAggregatesHLSL.cpp - Scalar Replacement of Aggregates ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// Based on ScalarReplAggregates.cpp. The difference is HLSL version will keep
// array so it can break up all structure.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/Loads.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Transforms/Utils/Local.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include <deque>
#include <unordered_map>

using namespace llvm;
using namespace hlsl;
#define DEBUG_TYPE "scalarreplhlsl"

STATISTIC(NumReplaced, "Number of allocas broken up");
STATISTIC(NumPromoted, "Number of allocas promoted");
STATISTIC(NumAdjusted, "Number of scalar allocas adjusted to allow promotion");
STATISTIC(NumConverted, "Number of aggregates converted to scalar");

namespace {

class SROA_Helper {
public:
  // Split V into AllocaInsts with Builder and save the new AllocaInsts into Elts.
  // Then do SROA on V.
  static bool DoScalarReplacement(Value *V, std::vector<Value *> &Elts,
                                  IRBuilder<> &Builder, bool bFlatVector,
                                  bool hasPrecise, DxilTypeSystem &typeSys,
                                  SmallVector<Value *, 32> &DeadInsts);

  static bool DoScalarReplacement(GlobalVariable *GV, std::vector<Value *> &Elts,
                                  IRBuilder<> &Builder, bool bFlatVector,
                                  bool hasPrecise, DxilTypeSystem &typeSys,
                                  SmallVector<Value *, 32> &DeadInsts);

  static void MarkEmptyStructUsers(Value *V, SmallVector<Value *, 32> &DeadInsts);
  static bool IsEmptyStructType(Type *Ty, DxilTypeSystem &typeSys);
private:
  SROA_Helper(Value *V, ArrayRef<Value *> Elts,
              SmallVector<Value *, 32> &DeadInsts)
      : OldVal(V), NewElts(Elts), DeadInsts(DeadInsts) {}
  void RewriteForScalarRepl(Value *V, IRBuilder<> &Builder);

private:
  // Must be a pointer type val.
  Value * OldVal;
  // Flattened elements for OldVal.
  ArrayRef<Value*> NewElts;
  SmallVector<Value *, 32> &DeadInsts;

  void RewriteForConstExpr(ConstantExpr *user, IRBuilder<> &Builder);
  void RewriteForGEP(GEPOperator *GEP, IRBuilder<> &Builder);
  void RewriteForLoad(LoadInst *loadInst);
  void RewriteForStore(StoreInst *storeInst);
  void RewriteMemIntrin(MemIntrinsic *MI, Instruction *Inst);
  void RewriteCall(CallInst *CI);
  void RewriteBitCast(BitCastInst *BCI);
};

struct SROA_HLSL : public FunctionPass {
  SROA_HLSL(bool Promote, int T, bool hasDT, char &ID, int ST, int AT, int SLT)
      : FunctionPass(ID), HasDomTree(hasDT), RunPromotion(Promote) {

    if (AT == -1)
      ArrayElementThreshold = 8;
    else
      ArrayElementThreshold = AT;
    if (SLT == -1)
      // Do not limit the scalar integer load size if no threshold is given.
      ScalarLoadThreshold = -1;
    else
      ScalarLoadThreshold = SLT;
  }

  bool runOnFunction(Function &F) override;

  bool performScalarRepl(Function &F);
  bool performPromotion(Function &F);
  bool markPrecise(Function &F);

private:
  bool HasDomTree;
  bool RunPromotion;

  /// DeadInsts - Keep track of instructions we have made dead, so that
  /// we can remove them after we are done working.
  SmallVector<Value *, 32> DeadInsts;

  /// AllocaInfo - When analyzing uses of an alloca instruction, this captures
  /// information about the uses.  All these fields are initialized to false
  /// and set to true when something is learned.
  struct AllocaInfo {
    /// The alloca to promote.
    AllocaInst *AI;

    /// CheckedPHIs - This is a set of verified PHI nodes, to prevent infinite
    /// looping and avoid redundant work.
    SmallPtrSet<PHINode *, 8> CheckedPHIs;

    /// isUnsafe - This is set to true if the alloca cannot be SROA'd.
    bool isUnsafe : 1;

    /// isMemCpySrc - This is true if this aggregate is memcpy'd from.
    bool isMemCpySrc : 1;

    /// isMemCpyDst - This is true if this aggregate is memcpy'd into.
    bool isMemCpyDst : 1;

    /// hasSubelementAccess - This is true if a subelement of the alloca is
    /// ever accessed, or false if the alloca is only accessed with mem
    /// intrinsics or load/store that only access the entire alloca at once.
    bool hasSubelementAccess : 1;

    /// hasALoadOrStore - This is true if there are any loads or stores to it.
    /// The alloca may just be accessed with memcpy, for example, which would
    /// not set this.
    bool hasALoadOrStore : 1;

    /// hasArrayIndexing - This is true if there are any dynamic array
    /// indexing to it.
    bool hasArrayIndexing : 1;

    /// hasVectorIndexing - This is true if there are any dynamic vector
    /// indexing to it.
    bool hasVectorIndexing : 1;

    explicit AllocaInfo(AllocaInst *ai)
        : AI(ai), isUnsafe(false), isMemCpySrc(false), isMemCpyDst(false),
          hasSubelementAccess(false), hasALoadOrStore(false),
          hasArrayIndexing(false), hasVectorIndexing(false) {}
  };

  /// ArrayElementThreshold - The maximum number of elements an array can
  /// have to be considered for SROA.
  unsigned ArrayElementThreshold;

  /// ScalarLoadThreshold - The maximum size in bits of scalars to load when
  /// converting to scalar
  unsigned ScalarLoadThreshold;

  void MarkUnsafe(AllocaInfo &I, Instruction *User) {
    I.isUnsafe = true;
    DEBUG(dbgs() << "  Transformation preventing inst: " << *User << '\n');
  }

  bool isSafeAllocaToScalarRepl(AllocaInst *AI);

  void isSafeForScalarRepl(Instruction *I, uint64_t Offset, AllocaInfo &Info);
  void isSafePHISelectUseForScalarRepl(Instruction *User, uint64_t Offset,
                                       AllocaInfo &Info);
  void isSafeGEP(GetElementPtrInst *GEPI, uint64_t &Offset, AllocaInfo &Info);
  void isSafeMemAccess(uint64_t Offset, uint64_t MemSize, Type *MemOpType,
                       bool isStore, AllocaInfo &Info, Instruction *TheAccess,
                       bool AllowWholeAccess);
  bool TypeHasComponent(Type *T, uint64_t Offset, uint64_t Size,
                        const DataLayout &DL);

  void DeleteDeadInstructions();

  bool ShouldAttemptScalarRepl(AllocaInst *AI);
};

// SROA_DT - SROA that uses DominatorTree.
struct SROA_DT_HLSL : public SROA_HLSL {
  static char ID;

public:
  SROA_DT_HLSL(bool Promote = false, int T = -1, int ST = -1, int AT = -1, int SLT = -1)
      : SROA_HLSL(Promote, T, true, ID, ST, AT, SLT) {
    initializeSROA_DTPass(*PassRegistry::getPassRegistry());
  }

  // getAnalysisUsage - This pass does not require any passes, but we know it
  // will not alter the CFG, so say so.
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
  }
};

// SROA_SSAUp - SROA that uses SSAUpdater.
struct SROA_SSAUp_HLSL : public SROA_HLSL {
  static char ID;

public:
  SROA_SSAUp_HLSL(bool Promote = false, int T = -1, int ST = -1, int AT = -1, int SLT = -1)
      : SROA_HLSL(Promote, T, false, ID, ST, AT, SLT) {
    initializeSROA_SSAUpPass(*PassRegistry::getPassRegistry());
  }

  // getAnalysisUsage - This pass does not require any passes, but we know it
  // will not alter the CFG, so say so.
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AssumptionCacheTracker>();
    AU.setPreservesCFG();
  }
};

// Simple struct to split memcpy into ld/st
struct MemcpySplitter {
  llvm::LLVMContext &m_context;
public:
  MemcpySplitter(llvm::LLVMContext &context) : m_context(context) {}
  void Split(llvm::Function &F);
};

}

char SROA_DT_HLSL::ID = 0;
char SROA_SSAUp_HLSL::ID = 0;

INITIALIZE_PASS_BEGIN(SROA_DT_HLSL, "scalarreplhlsl",
                      "Scalar Replacement of Aggregates HLSL (DT)", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(SROA_DT_HLSL, "scalarreplhlsl",
                    "Scalar Replacement of Aggregates HLSL (DT)", false, false)

INITIALIZE_PASS_BEGIN(SROA_SSAUp_HLSL, "scalarreplhlsl-ssa",
                      "Scalar Replacement of Aggregates HLSL (SSAUp)", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_END(SROA_SSAUp_HLSL, "scalarreplhlsl-ssa",
                    "Scalar Replacement of Aggregates HLSL (SSAUp)", false,
                    false)

// Public interface to the ScalarReplAggregates pass
FunctionPass *llvm::createScalarReplAggregatesHLSLPass(bool UseDomTree, bool Promote) {
  if (UseDomTree)
    return new SROA_DT_HLSL(Promote);
  return new SROA_SSAUp_HLSL(Promote);
}

//===----------------------------------------------------------------------===//
// Convert To Scalar Optimization.
//===----------------------------------------------------------------------===//

namespace {
/// ConvertToScalarInfo - This class implements the "Convert To Scalar"
/// optimization, which scans the uses of an alloca and determines if it can
/// rewrite it in terms of a single new alloca that can be mem2reg'd.
class ConvertToScalarInfo {
  /// AllocaSize - The size of the alloca being considered in bytes.
  unsigned AllocaSize;
  const DataLayout &DL;
  unsigned ScalarLoadThreshold;

  /// IsNotTrivial - This is set to true if there is some access to the object
  /// which means that mem2reg can't promote it.
  bool IsNotTrivial;

  /// ScalarKind - Tracks the kind of alloca being considered for promotion,
  /// computed based on the uses of the alloca rather than the LLVM type system.
  enum {
    Unknown,

    // Accesses via GEPs that are consistent with element access of a vector
    // type. This will not be converted into a vector unless there is a later
    // access using an actual vector type.
    ImplicitVector,

    // Accesses via vector operations and GEPs that are consistent with the
    // layout of a vector type.
    Vector,

    // An integer bag-of-bits with bitwise operations for insertion and
    // extraction. Any combination of types can be converted into this kind
    // of scalar.
    Integer
  } ScalarKind;

  /// VectorTy - This tracks the type that we should promote the vector to if
  /// it is possible to turn it into a vector.  This starts out null, and if it
  /// isn't possible to turn into a vector type, it gets set to VoidTy.
  VectorType *VectorTy;

  /// HadNonMemTransferAccess - True if there is at least one access to the
  /// alloca that is not a MemTransferInst.  We don't want to turn structs into
  /// large integers unless there is some potential for optimization.
  bool HadNonMemTransferAccess;

  /// HadDynamicAccess - True if some element of this alloca was dynamic.
  /// We don't yet have support for turning a dynamic access into a large
  /// integer.
  bool HadDynamicAccess;

public:
  explicit ConvertToScalarInfo(unsigned Size, const DataLayout &DL,
                               unsigned SLT)
      : AllocaSize(Size), DL(DL), ScalarLoadThreshold(SLT), IsNotTrivial(false),
        ScalarKind(Unknown), VectorTy(nullptr), HadNonMemTransferAccess(false),
        HadDynamicAccess(false) {}

  AllocaInst *TryConvert(AllocaInst *AI);

private:
  bool CanConvertToScalar(Value *V, uint64_t Offset, Value *NonConstantIdx);
  void MergeInTypeForLoadOrStore(Type *In, uint64_t Offset);
  bool MergeInVectorType(VectorType *VInTy, uint64_t Offset);
  void ConvertUsesToScalar(Value *Ptr, AllocaInst *NewAI, uint64_t Offset,
                           Value *NonConstantIdx);

  Value *ConvertScalar_ExtractValue(Value *NV, Type *ToType, uint64_t Offset,
                                    Value *NonConstantIdx,
                                    IRBuilder<> &Builder);
  Value *ConvertScalar_InsertValue(Value *StoredVal, Value *ExistingVal,
                                   uint64_t Offset, Value *NonConstantIdx,
                                   IRBuilder<> &Builder);
};
} // end anonymous namespace.

/// TryConvert - Analyze the specified alloca, and if it is safe to do so,
/// rewrite it to be a new alloca which is mem2reg'able.  This returns the new
/// alloca if possible or null if not.
AllocaInst *ConvertToScalarInfo::TryConvert(AllocaInst *AI) {
  // If we can't convert this scalar, or if mem2reg can trivially do it, bail
  // out.
  if (!CanConvertToScalar(AI, 0, nullptr) || !IsNotTrivial)
    return nullptr;

  // If an alloca has only memset / memcpy uses, it may still have an Unknown
  // ScalarKind. Treat it as an Integer below.
  if (ScalarKind == Unknown)
    ScalarKind = Integer;

  if (ScalarKind == Vector && VectorTy->getBitWidth() != AllocaSize * 8)
    ScalarKind = Integer;

  // If we were able to find a vector type that can handle this with
  // insert/extract elements, and if there was at least one use that had
  // a vector type, promote this to a vector.  We don't want to promote
  // random stuff that doesn't use vectors (e.g. <9 x double>) because then
  // we just get a lot of insert/extracts.  If at least one vector is
  // involved, then we probably really do have a union of vector/array.
  Type *NewTy;
  if (ScalarKind == Vector) {
    assert(VectorTy && "Missing type for vector scalar.");
    DEBUG(dbgs() << "CONVERT TO VECTOR: " << *AI << "\n  TYPE = " << *VectorTy
                 << '\n');
    NewTy = VectorTy; // Use the vector type.
  } else {
    unsigned BitWidth = AllocaSize * 8;

    // Do not convert to scalar integer if the alloca size exceeds the
    // scalar load threshold.
    if (BitWidth > ScalarLoadThreshold)
      return nullptr;

    if ((ScalarKind == ImplicitVector || ScalarKind == Integer) &&
        !HadNonMemTransferAccess && !DL.fitsInLegalInteger(BitWidth))
      return nullptr;
    // Dynamic accesses on integers aren't yet supported.  They need us to shift
    // by a dynamic amount which could be difficult to work out as we might not
    // know whether to use a left or right shift.
    if (ScalarKind == Integer && HadDynamicAccess)
      return nullptr;

    DEBUG(dbgs() << "CONVERT TO SCALAR INTEGER: " << *AI << "\n");
    // Create and insert the integer alloca.
    NewTy = IntegerType::get(AI->getContext(), BitWidth);
  }
  AllocaInst *NewAI =
      new AllocaInst(NewTy, nullptr, "", AI->getParent()->begin());
  ConvertUsesToScalar(AI, NewAI, 0, nullptr);
  return NewAI;
}

/// MergeInTypeForLoadOrStore - Add the 'In' type to the accumulated vector type
/// (VectorTy) so far at the offset specified by Offset (which is specified in
/// bytes).
///
/// There are two cases we handle here:
///   1) A union of vector types of the same size and potentially its elements.
///      Here we turn element accesses into insert/extract element operations.
///      This promotes a <4 x float> with a store of float to the third element
///      into a <4 x float> that uses insert element.
///   2) A fully general blob of memory, which we turn into some (potentially
///      large) integer type with extract and insert operations where the loads
///      and stores would mutate the memory.  We mark this by setting VectorTy
///      to VoidTy.
void ConvertToScalarInfo::MergeInTypeForLoadOrStore(Type *In, uint64_t Offset) {
  // If we already decided to turn this into a blob of integer memory, there is
  // nothing to be done.
  if (ScalarKind == Integer)
    return;

  // If this could be contributing to a vector, analyze it.

  // If the In type is a vector that is the same size as the alloca, see if it
  // matches the existing VecTy.
  if (VectorType *VInTy = dyn_cast<VectorType>(In)) {
    if (MergeInVectorType(VInTy, Offset))
      return;
  } else if (In->isFloatTy() || In->isDoubleTy() ||
             (In->isIntegerTy() && In->getPrimitiveSizeInBits() >= 8 &&
              isPowerOf2_32(In->getPrimitiveSizeInBits()))) {
    // Full width accesses can be ignored, because they can always be turned
    // into bitcasts.
    unsigned EltSize = In->getPrimitiveSizeInBits() / 8;
    if (EltSize == AllocaSize)
      return;

    // If we're accessing something that could be an element of a vector, see
    // if the implied vector agrees with what we already have and if Offset is
    // compatible with it.
    if (Offset % EltSize == 0 && AllocaSize % EltSize == 0 &&
        (!VectorTy ||
         EltSize == VectorTy->getElementType()->getPrimitiveSizeInBits() / 8)) {
      if (!VectorTy) {
        ScalarKind = ImplicitVector;
        VectorTy = VectorType::get(In, AllocaSize / EltSize);
      }
      return;
    }
  }

  // Otherwise, we have a case that we can't handle with an optimized vector
  // form.  We can still turn this into a large integer.
  ScalarKind = Integer;
}

/// MergeInVectorType - Handles the vector case of MergeInTypeForLoadOrStore,
/// returning true if the type was successfully merged and false otherwise.
bool ConvertToScalarInfo::MergeInVectorType(VectorType *VInTy,
                                            uint64_t Offset) {
  if (VInTy->getBitWidth() / 8 == AllocaSize && Offset == 0) {
    // If we're storing/loading a vector of the right size, allow it as a
    // vector.  If this the first vector we see, remember the type so that
    // we know the element size. If this is a subsequent access, ignore it
    // even if it is a differing type but the same size. Worst case we can
    // bitcast the resultant vectors.
    if (!VectorTy)
      VectorTy = VInTy;
    ScalarKind = Vector;
    return true;
  }

  return false;
}

/// CanConvertToScalar - V is a pointer.  If we can convert the pointee and all
/// its accesses to a single vector type, return true and set VecTy to
/// the new type.  If we could convert the alloca into a single promotable
/// integer, return true but set VecTy to VoidTy.  Further, if the use is not a
/// completely trivial use that mem2reg could promote, set IsNotTrivial.  Offset
/// is the current offset from the base of the alloca being analyzed.
///
/// If we see at least one access to the value that is as a vector type, set the
/// SawVec flag.
bool ConvertToScalarInfo::CanConvertToScalar(Value *V, uint64_t Offset,
                                             Value *NonConstantIdx) {
  for (User *U : V->users()) {
    Instruction *UI = cast<Instruction>(U);

    if (LoadInst *LI = dyn_cast<LoadInst>(UI)) {
      // Don't break volatile loads.
      if (!LI->isSimple())
        return false;

      HadNonMemTransferAccess = true;
      MergeInTypeForLoadOrStore(LI->getType(), Offset);
      continue;
    }

    if (StoreInst *SI = dyn_cast<StoreInst>(UI)) {
      // Storing the pointer, not into the value?
      if (SI->getOperand(0) == V || !SI->isSimple())
        return false;

      HadNonMemTransferAccess = true;
      MergeInTypeForLoadOrStore(SI->getOperand(0)->getType(), Offset);
      continue;
    }

    if (BitCastInst *BCI = dyn_cast<BitCastInst>(UI)) {
      if (!onlyUsedByLifetimeMarkers(BCI))
        IsNotTrivial = true; // Can't be mem2reg'd.
      if (!CanConvertToScalar(BCI, Offset, NonConstantIdx))
        return false;
      continue;
    }

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(UI)) {
      // If this is a GEP with a variable indices, we can't handle it.
      PointerType *PtrTy = dyn_cast<PointerType>(GEP->getPointerOperandType());
      if (!PtrTy)
        return false;

      // Compute the offset that this GEP adds to the pointer.
      SmallVector<Value *, 8> Indices(GEP->op_begin() + 1, GEP->op_end());
      Value *GEPNonConstantIdx = nullptr;
      if (!GEP->hasAllConstantIndices()) {
        if (!isa<VectorType>(PtrTy->getElementType()))
          return false;
        if (NonConstantIdx)
          return false;
        GEPNonConstantIdx = Indices.pop_back_val();
        if (!GEPNonConstantIdx->getType()->isIntegerTy(32))
          return false;
        HadDynamicAccess = true;
      } else
        GEPNonConstantIdx = NonConstantIdx;
      uint64_t GEPOffset = DL.getIndexedOffset(PtrTy, Indices);
      // See if all uses can be converted.
      if (!CanConvertToScalar(GEP, Offset + GEPOffset, GEPNonConstantIdx))
        return false;
      IsNotTrivial = true; // Can't be mem2reg'd.
      HadNonMemTransferAccess = true;
      continue;
    }

    // If this is a constant sized memset of a constant value (e.g. 0) we can
    // handle it.
    if (MemSetInst *MSI = dyn_cast<MemSetInst>(UI)) {
      // Store to dynamic index.
      if (NonConstantIdx)
        return false;
      // Store of constant value.
      if (!isa<ConstantInt>(MSI->getValue()))
        return false;

      // Store of constant size.
      ConstantInt *Len = dyn_cast<ConstantInt>(MSI->getLength());
      if (!Len)
        return false;

      // If the size differs from the alloca, we can only convert the alloca to
      // an integer bag-of-bits.
      // FIXME: This should handle all of the cases that are currently accepted
      // as vector element insertions.
      if (Len->getZExtValue() != AllocaSize || Offset != 0)
        ScalarKind = Integer;

      IsNotTrivial = true; // Can't be mem2reg'd.
      HadNonMemTransferAccess = true;
      continue;
    }

    // If this is a memcpy or memmove into or out of the whole allocation, we
    // can handle it like a load or store of the scalar type.
    if (MemTransferInst *MTI = dyn_cast<MemTransferInst>(UI)) {
      // Store to dynamic index.
      if (NonConstantIdx)
        return false;
      ConstantInt *Len = dyn_cast<ConstantInt>(MTI->getLength());
      if (!Len || Len->getZExtValue() != AllocaSize || Offset != 0)
        return false;

      IsNotTrivial = true; // Can't be mem2reg'd.
      continue;
    }

    // If this is a lifetime intrinsic, we can handle it.
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(UI)) {
      if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
          II->getIntrinsicID() == Intrinsic::lifetime_end) {
        continue;
      }
    }

    // Otherwise, we cannot handle this!
    return false;
  }

  return true;
}

/// ConvertUsesToScalar - Convert all of the users of Ptr to use the new alloca
/// directly.  This happens when we are converting an "integer union" to a
/// single integer scalar, or when we are converting a "vector union" to a
/// vector with insert/extractelement instructions.
///
/// Offset is an offset from the original alloca, in bits that need to be
/// shifted to the right.  By the end of this, there should be no uses of Ptr.
void ConvertToScalarInfo::ConvertUsesToScalar(Value *Ptr, AllocaInst *NewAI,
                                              uint64_t Offset,
                                              Value *NonConstantIdx) {
  while (!Ptr->use_empty()) {
    Instruction *User = cast<Instruction>(Ptr->user_back());

    if (BitCastInst *CI = dyn_cast<BitCastInst>(User)) {
      ConvertUsesToScalar(CI, NewAI, Offset, NonConstantIdx);
      CI->eraseFromParent();
      continue;
    }

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      // Compute the offset that this GEP adds to the pointer.
      SmallVector<Value *, 8> Indices(GEP->op_begin() + 1, GEP->op_end());
      Value *GEPNonConstantIdx = nullptr;
      if (!GEP->hasAllConstantIndices()) {
        assert(!NonConstantIdx &&
               "Dynamic GEP reading from dynamic GEP unsupported");
        GEPNonConstantIdx = Indices.pop_back_val();
      } else
        GEPNonConstantIdx = NonConstantIdx;
      uint64_t GEPOffset =
          DL.getIndexedOffset(GEP->getPointerOperandType(), Indices);
      ConvertUsesToScalar(GEP, NewAI, Offset + GEPOffset * 8,
                          GEPNonConstantIdx);
      GEP->eraseFromParent();
      continue;
    }

    IRBuilder<> Builder(User);

    if (LoadInst *LI = dyn_cast<LoadInst>(User)) {
      // The load is a bit extract from NewAI shifted right by Offset bits.
      Value *LoadedVal = Builder.CreateLoad(NewAI);
      Value *NewLoadVal = ConvertScalar_ExtractValue(
          LoadedVal, LI->getType(), Offset, NonConstantIdx, Builder);
      LI->replaceAllUsesWith(NewLoadVal);
      LI->eraseFromParent();
      continue;
    }

    if (StoreInst *SI = dyn_cast<StoreInst>(User)) {
      assert(SI->getOperand(0) != Ptr && "Consistency error!");
      Instruction *Old = Builder.CreateLoad(NewAI, NewAI->getName() + ".in");
      Value *New = ConvertScalar_InsertValue(SI->getOperand(0), Old, Offset,
                                             NonConstantIdx, Builder);
      Builder.CreateStore(New, NewAI);
      SI->eraseFromParent();

      // If the load we just inserted is now dead, then the inserted store
      // overwrote the entire thing.
      if (Old->use_empty())
        Old->eraseFromParent();
      continue;
    }

    // If this is a constant sized memset of a constant value (e.g. 0) we can
    // transform it into a store of the expanded constant value.
    if (MemSetInst *MSI = dyn_cast<MemSetInst>(User)) {
      assert(MSI->getRawDest() == Ptr && "Consistency error!");
      assert(!NonConstantIdx && "Cannot replace dynamic memset with insert");
      int64_t SNumBytes = cast<ConstantInt>(MSI->getLength())->getSExtValue();
      if (SNumBytes > 0 && (SNumBytes >> 32) == 0) {
        unsigned NumBytes = static_cast<unsigned>(SNumBytes);
        unsigned Val = cast<ConstantInt>(MSI->getValue())->getZExtValue();

        // Compute the value replicated the right number of times.
        APInt APVal(NumBytes * 8, Val);

        // Splat the value if non-zero.
        if (Val)
          for (unsigned i = 1; i != NumBytes; ++i)
            APVal |= APVal << 8;

        Instruction *Old = Builder.CreateLoad(NewAI, NewAI->getName() + ".in");
        Value *New = ConvertScalar_InsertValue(
            ConstantInt::get(User->getContext(), APVal), Old, Offset, nullptr,
            Builder);
        Builder.CreateStore(New, NewAI);

        // If the load we just inserted is now dead, then the memset overwrote
        // the entire thing.
        if (Old->use_empty())
          Old->eraseFromParent();
      }
      MSI->eraseFromParent();
      continue;
    }

    // If this is a memcpy or memmove into or out of the whole allocation, we
    // can handle it like a load or store of the scalar type.
    if (MemTransferInst *MTI = dyn_cast<MemTransferInst>(User)) {
      assert(Offset == 0 && "must be store to start of alloca");
      assert(!NonConstantIdx && "Cannot replace dynamic transfer with insert");

      // If the source and destination are both to the same alloca, then this is
      // a noop copy-to-self, just delete it.  Otherwise, emit a load and store
      // as appropriate.
      AllocaInst *OrigAI = cast<AllocaInst>(GetUnderlyingObject(Ptr, DL, 0));

      if (GetUnderlyingObject(MTI->getSource(), DL, 0) != OrigAI) {
        // Dest must be OrigAI, change this to be a load from the original
        // pointer (bitcasted), then a store to our new alloca.
        assert(MTI->getRawDest() == Ptr && "Neither use is of pointer?");
        Value *SrcPtr = MTI->getSource();
        PointerType *SPTy = cast<PointerType>(SrcPtr->getType());
        PointerType *AIPTy = cast<PointerType>(NewAI->getType());
        if (SPTy->getAddressSpace() != AIPTy->getAddressSpace()) {
          AIPTy = PointerType::get(AIPTy->getElementType(),
                                   SPTy->getAddressSpace());
        }
        SrcPtr = Builder.CreateBitCast(SrcPtr, AIPTy);

        LoadInst *SrcVal = Builder.CreateLoad(SrcPtr, "srcval");
        SrcVal->setAlignment(MTI->getAlignment());
        Builder.CreateStore(SrcVal, NewAI);
      } else if (GetUnderlyingObject(MTI->getDest(), DL, 0) != OrigAI) {
        // Src must be OrigAI, change this to be a load from NewAI then a store
        // through the original dest pointer (bitcasted).
        assert(MTI->getRawSource() == Ptr && "Neither use is of pointer?");
        LoadInst *SrcVal = Builder.CreateLoad(NewAI, "srcval");

        PointerType *DPTy = cast<PointerType>(MTI->getDest()->getType());
        PointerType *AIPTy = cast<PointerType>(NewAI->getType());
        if (DPTy->getAddressSpace() != AIPTy->getAddressSpace()) {
          AIPTy = PointerType::get(AIPTy->getElementType(),
                                   DPTy->getAddressSpace());
        }
        Value *DstPtr = Builder.CreateBitCast(MTI->getDest(), AIPTy);

        StoreInst *NewStore = Builder.CreateStore(SrcVal, DstPtr);
        NewStore->setAlignment(MTI->getAlignment());
      } else {
        // Noop transfer. Src == Dst
      }

      MTI->eraseFromParent();
      continue;
    }

    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(User)) {
      if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
          II->getIntrinsicID() == Intrinsic::lifetime_end) {
        // There's no need to preserve these, as the resulting alloca will be
        // converted to a register anyways.
        II->eraseFromParent();
        continue;
      }
    }

    llvm_unreachable("Unsupported operation!");
  }
}

/// ConvertScalar_ExtractValue - Extract a value of type ToType from an integer
/// or vector value FromVal, extracting the bits from the offset specified by
/// Offset.  This returns the value, which is of type ToType.
///
/// This happens when we are converting an "integer union" to a single
/// integer scalar, or when we are converting a "vector union" to a vector with
/// insert/extractelement instructions.
///
/// Offset is an offset from the original alloca, in bits that need to be
/// shifted to the right.
Value *ConvertToScalarInfo::ConvertScalar_ExtractValue(Value *FromVal,
                                                       Type *ToType,
                                                       uint64_t Offset,
                                                       Value *NonConstantIdx,
                                                       IRBuilder<> &Builder) {
  // If the load is of the whole new alloca, no conversion is needed.
  Type *FromType = FromVal->getType();
  if (FromType == ToType && Offset == 0)
    return FromVal;

  // If the result alloca is a vector type, this is either an element
  // access or a bitcast to another vector type of the same size.
  if (VectorType *VTy = dyn_cast<VectorType>(FromType)) {
    unsigned FromTypeSize = DL.getTypeAllocSize(FromType);
    unsigned ToTypeSize = DL.getTypeAllocSize(ToType);
    if (FromTypeSize == ToTypeSize)
      return Builder.CreateBitCast(FromVal, ToType);

    // Otherwise it must be an element access.
    unsigned Elt = 0;
    if (Offset) {
      unsigned EltSize = DL.getTypeAllocSizeInBits(VTy->getElementType());
      Elt = Offset / EltSize;
      assert(EltSize * Elt == Offset && "Invalid modulus in validity checking");
    }
    // Return the element extracted out of it.
    Value *Idx;
    if (NonConstantIdx) {
      if (Elt)
        Idx = Builder.CreateAdd(NonConstantIdx, Builder.getInt32(Elt),
                                "dyn.offset");
      else
        Idx = NonConstantIdx;
    } else
      Idx = Builder.getInt32(Elt);
    Value *V = Builder.CreateExtractElement(FromVal, Idx);
    if (V->getType() != ToType)
      V = Builder.CreateBitCast(V, ToType);
    return V;
  }

  // If ToType is a first class aggregate, extract out each of the pieces and
  // use insertvalue's to form the FCA.
  if (StructType *ST = dyn_cast<StructType>(ToType)) {
    assert(!NonConstantIdx &&
           "Dynamic indexing into struct types not supported");
    const StructLayout &Layout = *DL.getStructLayout(ST);
    Value *Res = UndefValue::get(ST);
    for (unsigned i = 0, e = ST->getNumElements(); i != e; ++i) {
      Value *Elt = ConvertScalar_ExtractValue(
          FromVal, ST->getElementType(i),
          Offset + Layout.getElementOffsetInBits(i), nullptr, Builder);
      Res = Builder.CreateInsertValue(Res, Elt, i);
    }
    return Res;
  }

  if (ArrayType *AT = dyn_cast<ArrayType>(ToType)) {
    assert(!NonConstantIdx &&
           "Dynamic indexing into array types not supported");
    uint64_t EltSize = DL.getTypeAllocSizeInBits(AT->getElementType());
    Value *Res = UndefValue::get(AT);
    for (unsigned i = 0, e = AT->getNumElements(); i != e; ++i) {
      Value *Elt =
          ConvertScalar_ExtractValue(FromVal, AT->getElementType(),
                                     Offset + i * EltSize, nullptr, Builder);
      Res = Builder.CreateInsertValue(Res, Elt, i);
    }
    return Res;
  }

  // Otherwise, this must be a union that was converted to an integer value.
  IntegerType *NTy = cast<IntegerType>(FromVal->getType());

  // If this is a big-endian system and the load is narrower than the
  // full alloca type, we need to do a shift to get the right bits.
  int ShAmt = 0;
  if (DL.isBigEndian()) {
    // On big-endian machines, the lowest bit is stored at the bit offset
    // from the pointer given by getTypeStoreSizeInBits.  This matters for
    // integers with a bitwidth that is not a multiple of 8.
    ShAmt = DL.getTypeStoreSizeInBits(NTy) - DL.getTypeStoreSizeInBits(ToType) -
            Offset;
  } else {
    ShAmt = Offset;
  }

  // Note: we support negative bitwidths (with shl) which are not defined.
  // We do this to support (f.e.) loads off the end of a structure where
  // only some bits are used.
  if (ShAmt > 0 && (unsigned)ShAmt < NTy->getBitWidth())
    FromVal = Builder.CreateLShr(FromVal,
                                 ConstantInt::get(FromVal->getType(), ShAmt));
  else if (ShAmt < 0 && (unsigned)-ShAmt < NTy->getBitWidth())
    FromVal = Builder.CreateShl(FromVal,
                                ConstantInt::get(FromVal->getType(), -ShAmt));

  // Finally, unconditionally truncate the integer to the right width.
  unsigned LIBitWidth = DL.getTypeSizeInBits(ToType);
  if (LIBitWidth < NTy->getBitWidth())
    FromVal = Builder.CreateTrunc(
        FromVal, IntegerType::get(FromVal->getContext(), LIBitWidth));
  else if (LIBitWidth > NTy->getBitWidth())
    FromVal = Builder.CreateZExt(
        FromVal, IntegerType::get(FromVal->getContext(), LIBitWidth));

  // If the result is an integer, this is a trunc or bitcast.
  if (ToType->isIntegerTy()) {
    // Should be done.
  } else if (ToType->isFloatingPointTy() || ToType->isVectorTy()) {
    // Just do a bitcast, we know the sizes match up.
    FromVal = Builder.CreateBitCast(FromVal, ToType);
  } else {
    // Otherwise must be a pointer.
    FromVal = Builder.CreateIntToPtr(FromVal, ToType);
  }
  assert(FromVal->getType() == ToType && "Didn't convert right?");
  return FromVal;
}

/// ConvertScalar_InsertValue - Insert the value "SV" into the existing integer
/// or vector value "Old" at the offset specified by Offset.
///
/// This happens when we are converting an "integer union" to a
/// single integer scalar, or when we are converting a "vector union" to a
/// vector with insert/extractelement instructions.
///
/// Offset is an offset from the original alloca, in bits that need to be
/// shifted to the right.
///
/// NonConstantIdx is an index value if there was a GEP with a non-constant
/// index value.  If this is 0 then all GEPs used to find this insert address
/// are constant.
Value *ConvertToScalarInfo::ConvertScalar_InsertValue(Value *SV, Value *Old,
                                                      uint64_t Offset,
                                                      Value *NonConstantIdx,
                                                      IRBuilder<> &Builder) {
  // Convert the stored type to the actual type, shift it left to insert
  // then 'or' into place.
  Type *AllocaType = Old->getType();
  LLVMContext &Context = Old->getContext();

  if (VectorType *VTy = dyn_cast<VectorType>(AllocaType)) {
    uint64_t VecSize = DL.getTypeAllocSizeInBits(VTy);
    uint64_t ValSize = DL.getTypeAllocSizeInBits(SV->getType());

    // Changing the whole vector with memset or with an access of a different
    // vector type?
    if (ValSize == VecSize)
      return Builder.CreateBitCast(SV, AllocaType);

    // Must be an element insertion.
    Type *EltTy = VTy->getElementType();
    if (SV->getType() != EltTy)
      SV = Builder.CreateBitCast(SV, EltTy);
    uint64_t EltSize = DL.getTypeAllocSizeInBits(EltTy);
    unsigned Elt = Offset / EltSize;
    Value *Idx;
    if (NonConstantIdx) {
      if (Elt)
        Idx = Builder.CreateAdd(NonConstantIdx, Builder.getInt32(Elt),
                                "dyn.offset");
      else
        Idx = NonConstantIdx;
    } else
      Idx = Builder.getInt32(Elt);
    return Builder.CreateInsertElement(Old, SV, Idx);
  }

  // If SV is a first-class aggregate value, insert each value recursively.
  if (StructType *ST = dyn_cast<StructType>(SV->getType())) {
    assert(!NonConstantIdx &&
           "Dynamic indexing into struct types not supported");
    const StructLayout &Layout = *DL.getStructLayout(ST);
    for (unsigned i = 0, e = ST->getNumElements(); i != e; ++i) {
      Value *Elt = Builder.CreateExtractValue(SV, i);
      Old = ConvertScalar_InsertValue(Elt, Old,
                                      Offset + Layout.getElementOffsetInBits(i),
                                      nullptr, Builder);
    }
    return Old;
  }

  if (ArrayType *AT = dyn_cast<ArrayType>(SV->getType())) {
    assert(!NonConstantIdx &&
           "Dynamic indexing into array types not supported");
    uint64_t EltSize = DL.getTypeAllocSizeInBits(AT->getElementType());
    for (unsigned i = 0, e = AT->getNumElements(); i != e; ++i) {
      Value *Elt = Builder.CreateExtractValue(SV, i);
      Old = ConvertScalar_InsertValue(Elt, Old, Offset + i * EltSize, nullptr,
                                      Builder);
    }
    return Old;
  }

  // If SV is a float, convert it to the appropriate integer type.
  // If it is a pointer, do the same.
  unsigned SrcWidth = DL.getTypeSizeInBits(SV->getType());
  unsigned DestWidth = DL.getTypeSizeInBits(AllocaType);
  unsigned SrcStoreWidth = DL.getTypeStoreSizeInBits(SV->getType());
  unsigned DestStoreWidth = DL.getTypeStoreSizeInBits(AllocaType);
  if (SV->getType()->isFloatingPointTy() || SV->getType()->isVectorTy())
    SV =
        Builder.CreateBitCast(SV, IntegerType::get(SV->getContext(), SrcWidth));
  else if (SV->getType()->isPointerTy())
    SV = Builder.CreatePtrToInt(SV, DL.getIntPtrType(SV->getType()));

  // Zero extend or truncate the value if needed.
  if (SV->getType() != AllocaType) {
    if (SV->getType()->getPrimitiveSizeInBits() <
        AllocaType->getPrimitiveSizeInBits())
      SV = Builder.CreateZExt(SV, AllocaType);
    else {
      // Truncation may be needed if storing more than the alloca can hold
      // (undefined behavior).
      SV = Builder.CreateTrunc(SV, AllocaType);
      SrcWidth = DestWidth;
      SrcStoreWidth = DestStoreWidth;
    }
  }

  // If this is a big-endian system and the store is narrower than the
  // full alloca type, we need to do a shift to get the right bits.
  int ShAmt = 0;
  if (DL.isBigEndian()) {
    // On big-endian machines, the lowest bit is stored at the bit offset
    // from the pointer given by getTypeStoreSizeInBits.  This matters for
    // integers with a bitwidth that is not a multiple of 8.
    ShAmt = DestStoreWidth - SrcStoreWidth - Offset;
  } else {
    ShAmt = Offset;
  }

  // Note: we support negative bitwidths (with shr) which are not defined.
  // We do this to support (f.e.) stores off the end of a structure where
  // only some bits in the structure are set.
  APInt Mask(APInt::getLowBitsSet(DestWidth, SrcWidth));
  if (ShAmt > 0 && (unsigned)ShAmt < DestWidth) {
    SV = Builder.CreateShl(SV, ConstantInt::get(SV->getType(), ShAmt));
    Mask <<= ShAmt;
  } else if (ShAmt < 0 && (unsigned)-ShAmt < DestWidth) {
    SV = Builder.CreateLShr(SV, ConstantInt::get(SV->getType(), -ShAmt));
    Mask = Mask.lshr(-ShAmt);
  }

  // Mask out the bits we are about to insert from the old value, and or
  // in the new bits.
  if (SrcWidth != DestWidth) {
    assert(DestWidth > SrcWidth);
    Old = Builder.CreateAnd(Old, ConstantInt::get(Context, ~Mask), "mask");
    SV = Builder.CreateOr(Old, SV, "ins");
  }
  return SV;
}

//===----------------------------------------------------------------------===//
// SRoA Driver
//===----------------------------------------------------------------------===//

bool SROA_HLSL::runOnFunction(Function &F) {
  // change memcpy into ld/st first
  MemcpySplitter splitter(F.getContext());
  splitter.Split(F);

  bool Changed = performScalarRepl(F);
  Changed |= markPrecise(F);
  Changed |= performPromotion(F);

  return Changed;
}

namespace {
class AllocaPromoter : public LoadAndStorePromoter {
  AllocaInst *AI;
  DIBuilder *DIB;
  SmallVector<DbgDeclareInst *, 4> DDIs;
  SmallVector<DbgValueInst *, 4> DVIs;

public:
  AllocaPromoter(ArrayRef<Instruction *> Insts, SSAUpdater &S, DIBuilder *DB)
      : LoadAndStorePromoter(Insts, S), AI(nullptr), DIB(DB) {}

  void run(AllocaInst *AI, const SmallVectorImpl<Instruction *> &Insts) {
    // Remember which alloca we're promoting (for isInstInList).
    this->AI = AI;
    if (auto *L = LocalAsMetadata::getIfExists(AI)) {
      if (auto *DINode = MetadataAsValue::getIfExists(AI->getContext(), L)) {
        for (User *U : DINode->users())
          if (DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(U))
            DDIs.push_back(DDI);
          else if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U))
            DVIs.push_back(DVI);
      }
    }

    LoadAndStorePromoter::run(Insts);
    AI->eraseFromParent();
    for (SmallVectorImpl<DbgDeclareInst *>::iterator I = DDIs.begin(),
                                                     E = DDIs.end();
         I != E; ++I) {
      DbgDeclareInst *DDI = *I;
      DDI->eraseFromParent();
    }
    for (SmallVectorImpl<DbgValueInst *>::iterator I = DVIs.begin(),
                                                   E = DVIs.end();
         I != E; ++I) {
      DbgValueInst *DVI = *I;
      DVI->eraseFromParent();
    }
  }

  bool
  isInstInList(Instruction *I,
               const SmallVectorImpl<Instruction *> &Insts) const override {
    if (LoadInst *LI = dyn_cast<LoadInst>(I))
      return LI->getOperand(0) == AI;
    return cast<StoreInst>(I)->getPointerOperand() == AI;
  }

  void updateDebugInfo(Instruction *Inst) const override {
    for (SmallVectorImpl<DbgDeclareInst *>::const_iterator I = DDIs.begin(),
                                                           E = DDIs.end();
         I != E; ++I) {
      DbgDeclareInst *DDI = *I;
      if (StoreInst *SI = dyn_cast<StoreInst>(Inst))
        ConvertDebugDeclareToDebugValue(DDI, SI, *DIB);
      else if (LoadInst *LI = dyn_cast<LoadInst>(Inst))
        ConvertDebugDeclareToDebugValue(DDI, LI, *DIB);
    }
    for (SmallVectorImpl<DbgValueInst *>::const_iterator I = DVIs.begin(),
                                                         E = DVIs.end();
         I != E; ++I) {
      DbgValueInst *DVI = *I;
      Value *Arg = nullptr;
      if (StoreInst *SI = dyn_cast<StoreInst>(Inst)) {
        // If an argument is zero extended then use argument directly. The ZExt
        // may be zapped by an optimization pass in future.
        if (ZExtInst *ZExt = dyn_cast<ZExtInst>(SI->getOperand(0)))
          Arg = dyn_cast<Argument>(ZExt->getOperand(0));
        if (SExtInst *SExt = dyn_cast<SExtInst>(SI->getOperand(0)))
          Arg = dyn_cast<Argument>(SExt->getOperand(0));
        if (!Arg)
          Arg = SI->getOperand(0);
      } else if (LoadInst *LI = dyn_cast<LoadInst>(Inst)) {
        Arg = LI->getOperand(0);
      } else {
        continue;
      }
      DIB->insertDbgValueIntrinsic(Arg, 0, DVI->getVariable(),
                                   DVI->getExpression(), DVI->getDebugLoc(),
                                   Inst);
    }
  }
};
} // end anon namespace

/// isSafeSelectToSpeculate - Select instructions that use an alloca and are
/// subsequently loaded can be rewritten to load both input pointers and then
/// select between the result, allowing the load of the alloca to be promoted.
/// From this:
///   %P2 = select i1 %cond, i32* %Alloca, i32* %Other
///   %V = load i32* %P2
/// to:
///   %V1 = load i32* %Alloca      -> will be mem2reg'd
///   %V2 = load i32* %Other
///   %V = select i1 %cond, i32 %V1, i32 %V2
///
/// We can do this to a select if its only uses are loads and if the operand to
/// the select can be loaded unconditionally.
static bool isSafeSelectToSpeculate(SelectInst *SI) {
  const DataLayout &DL = SI->getModule()->getDataLayout();
  bool TDerefable = isDereferenceablePointer(SI->getTrueValue(), DL);
  bool FDerefable = isDereferenceablePointer(SI->getFalseValue(), DL);

  for (User *U : SI->users()) {
    LoadInst *LI = dyn_cast<LoadInst>(U);
    if (!LI || !LI->isSimple())
      return false;

    // Both operands to the select need to be dereferencable, either absolutely
    // (e.g. allocas) or at this point because we can see other accesses to it.
    if (!TDerefable &&
        !isSafeToLoadUnconditionally(SI->getTrueValue(), LI,
                                     LI->getAlignment()))
      return false;
    if (!FDerefable &&
        !isSafeToLoadUnconditionally(SI->getFalseValue(), LI,
                                     LI->getAlignment()))
      return false;
  }

  return true;
}

/// isSafePHIToSpeculate - PHI instructions that use an alloca and are
/// subsequently loaded can be rewritten to load both input pointers in the pred
/// blocks and then PHI the results, allowing the load of the alloca to be
/// promoted.
/// From this:
///   %P2 = phi [i32* %Alloca, i32* %Other]
///   %V = load i32* %P2
/// to:
///   %V1 = load i32* %Alloca      -> will be mem2reg'd
///   ...
///   %V2 = load i32* %Other
///   ...
///   %V = phi [i32 %V1, i32 %V2]
///
/// We can do this to a select if its only uses are loads and if the operand to
/// the select can be loaded unconditionally.
static bool isSafePHIToSpeculate(PHINode *PN) {
  // For now, we can only do this promotion if the load is in the same block as
  // the PHI, and if there are no stores between the phi and load.
  // TODO: Allow recursive phi users.
  // TODO: Allow stores.
  BasicBlock *BB = PN->getParent();
  unsigned MaxAlign = 0;
  for (User *U : PN->users()) {
    LoadInst *LI = dyn_cast<LoadInst>(U);
    if (!LI || !LI->isSimple())
      return false;

    // For now we only allow loads in the same block as the PHI.  This is a
    // common case that happens when instcombine merges two loads through a PHI.
    if (LI->getParent() != BB)
      return false;

    // Ensure that there are no instructions between the PHI and the load that
    // could store.
    for (BasicBlock::iterator BBI = PN; &*BBI != LI; ++BBI)
      if (BBI->mayWriteToMemory())
        return false;

    MaxAlign = std::max(MaxAlign, LI->getAlignment());
  }

  const DataLayout &DL = PN->getModule()->getDataLayout();

  // Okay, we know that we have one or more loads in the same block as the PHI.
  // We can transform this if it is safe to push the loads into the predecessor
  // blocks.  The only thing to watch out for is that we can't put a possibly
  // trapping load in the predecessor if it is a critical edge.
  for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
    BasicBlock *Pred = PN->getIncomingBlock(i);
    Value *InVal = PN->getIncomingValue(i);

    // If the terminator of the predecessor has side-effects (an invoke),
    // there is no safe place to put a load in the predecessor.
    if (Pred->getTerminator()->mayHaveSideEffects())
      return false;

    // If the value is produced by the terminator of the predecessor
    // (an invoke), there is no valid place to put a load in the predecessor.
    if (Pred->getTerminator() == InVal)
      return false;

    // If the predecessor has a single successor, then the edge isn't critical.
    if (Pred->getTerminator()->getNumSuccessors() == 1)
      continue;

    // If this pointer is always safe to load, or if we can prove that there is
    // already a load in the block, then we can move the load to the pred block.
    if (isDereferenceablePointer(InVal, DL) ||
        isSafeToLoadUnconditionally(InVal, Pred->getTerminator(), MaxAlign))
      continue;

    return false;
  }

  return true;
}

/// tryToMakeAllocaBePromotable - This returns true if the alloca only has
/// direct (non-volatile) loads and stores to it.  If the alloca is close but
/// not quite there, this will transform the code to allow promotion.  As such,
/// it is a non-pure predicate.
static bool tryToMakeAllocaBePromotable(AllocaInst *AI, const DataLayout &DL) {
  SetVector<Instruction *, SmallVector<Instruction *, 4>,
            SmallPtrSet<Instruction *, 4>>
      InstsToRewrite;
  for (User *U : AI->users()) {
    if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
      if (!LI->isSimple())
        return false;
      continue;
    }

    if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
      if (SI->getOperand(0) == AI || !SI->isSimple())
        return false; // Don't allow a store OF the AI, only INTO the AI.
      continue;
    }

    if (SelectInst *SI = dyn_cast<SelectInst>(U)) {
      // If the condition being selected on is a constant, fold the select, yes
      // this does (rarely) happen early on.
      if (ConstantInt *CI = dyn_cast<ConstantInt>(SI->getCondition())) {
        Value *Result = SI->getOperand(1 + CI->isZero());
        SI->replaceAllUsesWith(Result);
        SI->eraseFromParent();

        // This is very rare and we just scrambled the use list of AI, start
        // over completely.
        return tryToMakeAllocaBePromotable(AI, DL);
      }

      // If it is safe to turn "load (select c, AI, ptr)" into a select of two
      // loads, then we can transform this by rewriting the select.
      if (!isSafeSelectToSpeculate(SI))
        return false;

      InstsToRewrite.insert(SI);
      continue;
    }

    if (PHINode *PN = dyn_cast<PHINode>(U)) {
      if (PN->use_empty()) { // Dead PHIs can be stripped.
        InstsToRewrite.insert(PN);
        continue;
      }

      // If it is safe to turn "load (phi [AI, ptr, ...])" into a PHI of loads
      // in the pred blocks, then we can transform this by rewriting the PHI.
      if (!isSafePHIToSpeculate(PN))
        return false;

      InstsToRewrite.insert(PN);
      continue;
    }

    if (BitCastInst *BCI = dyn_cast<BitCastInst>(U)) {
      if (onlyUsedByLifetimeMarkers(BCI)) {
        InstsToRewrite.insert(BCI);
        continue;
      }
    }

    return false;
  }

  // If there are no instructions to rewrite, then all uses are load/stores and
  // we're done!
  if (InstsToRewrite.empty())
    return true;

  // If we have instructions that need to be rewritten for this to be promotable
  // take care of it now.
  for (unsigned i = 0, e = InstsToRewrite.size(); i != e; ++i) {
    if (BitCastInst *BCI = dyn_cast<BitCastInst>(InstsToRewrite[i])) {
      // This could only be a bitcast used by nothing but lifetime intrinsics.
      for (BitCastInst::user_iterator I = BCI->user_begin(),
                                      E = BCI->user_end();
           I != E;)
        cast<Instruction>(*I++)->eraseFromParent();
      BCI->eraseFromParent();
      continue;
    }

    if (SelectInst *SI = dyn_cast<SelectInst>(InstsToRewrite[i])) {
      // Selects in InstsToRewrite only have load uses.  Rewrite each as two
      // loads with a new select.
      while (!SI->use_empty()) {
        LoadInst *LI = cast<LoadInst>(SI->user_back());

        IRBuilder<> Builder(LI);
        LoadInst *TrueLoad =
            Builder.CreateLoad(SI->getTrueValue(), LI->getName() + ".t");
        LoadInst *FalseLoad =
            Builder.CreateLoad(SI->getFalseValue(), LI->getName() + ".f");

        // Transfer alignment and AA info if present.
        TrueLoad->setAlignment(LI->getAlignment());
        FalseLoad->setAlignment(LI->getAlignment());

        AAMDNodes Tags;
        LI->getAAMetadata(Tags);
        if (Tags) {
          TrueLoad->setAAMetadata(Tags);
          FalseLoad->setAAMetadata(Tags);
        }

        Value *V =
            Builder.CreateSelect(SI->getCondition(), TrueLoad, FalseLoad);
        V->takeName(LI);
        LI->replaceAllUsesWith(V);
        LI->eraseFromParent();
      }

      // Now that all the loads are gone, the select is gone too.
      SI->eraseFromParent();
      continue;
    }

    // Otherwise, we have a PHI node which allows us to push the loads into the
    // predecessors.
    PHINode *PN = cast<PHINode>(InstsToRewrite[i]);
    if (PN->use_empty()) {
      PN->eraseFromParent();
      continue;
    }

    Type *LoadTy = cast<PointerType>(PN->getType())->getElementType();
    PHINode *NewPN = PHINode::Create(LoadTy, PN->getNumIncomingValues(),
                                     PN->getName() + ".ld", PN);

    // Get the AA tags and alignment to use from one of the loads.  It doesn't
    // matter which one we get and if any differ, it doesn't matter.
    LoadInst *SomeLoad = cast<LoadInst>(PN->user_back());

    AAMDNodes AATags;
    SomeLoad->getAAMetadata(AATags);
    unsigned Align = SomeLoad->getAlignment();

    // Rewrite all loads of the PN to use the new PHI.
    while (!PN->use_empty()) {
      LoadInst *LI = cast<LoadInst>(PN->user_back());
      LI->replaceAllUsesWith(NewPN);
      LI->eraseFromParent();
    }

    // Inject loads into all of the pred blocks.  Keep track of which blocks we
    // insert them into in case we have multiple edges from the same block.
    DenseMap<BasicBlock *, LoadInst *> InsertedLoads;

    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      BasicBlock *Pred = PN->getIncomingBlock(i);
      LoadInst *&Load = InsertedLoads[Pred];
      if (!Load) {
        Load = new LoadInst(PN->getIncomingValue(i),
                            PN->getName() + "." + Pred->getName(),
                            Pred->getTerminator());
        Load->setAlignment(Align);
        if (AATags)
          Load->setAAMetadata(AATags);
      }

      NewPN->addIncoming(Load, Pred);
    }

    PN->eraseFromParent();
  }

  ++NumAdjusted;
  return true;
}

bool SROA_HLSL::performPromotion(Function &F) {
  std::vector<AllocaInst *> Allocas;
  const DataLayout &DL = F.getParent()->getDataLayout();
  DominatorTree *DT = nullptr;
  if (HasDomTree)
    DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache &AC =
      getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);

  BasicBlock &BB = F.getEntryBlock(); // Get the entry node for the function
  DIBuilder DIB(*F.getParent(), /*AllowUnresolved*/ false);
  bool Changed = false;
  SmallVector<Instruction *, 64> Insts;
  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) { // Is it an alloca?
        DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(AI);
        // Skip alloca has debug info when not promote.
        if (DDI && !RunPromotion) {
          continue;
        }
        if (tryToMakeAllocaBePromotable(AI, DL))
          Allocas.push_back(AI);
      }
    if (Allocas.empty())
      break;

    if (HasDomTree)
      PromoteMemToReg(Allocas, *DT, nullptr, &AC);
    else {
      SSAUpdater SSA;
      for (unsigned i = 0, e = Allocas.size(); i != e; ++i) {
        AllocaInst *AI = Allocas[i];

        // Build list of instructions to promote.
        for (User *U : AI->users())
          Insts.push_back(cast<Instruction>(U));
        AllocaPromoter(Insts, SSA, &DIB).run(AI, Insts);
        Insts.clear();
      }
    }
    NumPromoted += Allocas.size();
    Changed = true;
  }

  return Changed;
}

/// ShouldAttemptScalarRepl - Decide if an alloca is a good candidate for
/// SROA.  It must be a struct or array type with a small number of elements.
bool SROA_HLSL::ShouldAttemptScalarRepl(AllocaInst *AI) {
  Type *T = AI->getAllocatedType();
  // promote every struct.
  if (StructType *ST = dyn_cast<StructType>(T))
    return true;
  // promote every array.
  if (ArrayType *AT = dyn_cast<ArrayType>(T))
    return true;
  return false;
}

static Value *MergeGEP(GEPOperator *SrcGEP, GetElementPtrInst *GEP) {
  IRBuilder<> Builder(GEP);
  SmallVector<Value *, 8> Indices;

  // Find out whether the last index in the source GEP is a sequential idx.
  bool EndsWithSequential = false;
  for (gep_type_iterator I = gep_type_begin(*SrcGEP), E = gep_type_end(*SrcGEP);
       I != E; ++I)
    EndsWithSequential = !(*I)->isStructTy();
  if (EndsWithSequential) {
    Value *Sum;
    Value *SO1 = SrcGEP->getOperand(SrcGEP->getNumOperands() - 1);
    Value *GO1 = GEP->getOperand(1);
    if (SO1 == Constant::getNullValue(SO1->getType())) {
      Sum = GO1;
    } else if (GO1 == Constant::getNullValue(GO1->getType())) {
      Sum = SO1;
    } else {
      // If they aren't the same type, then the input hasn't been processed
      // by the loop above yet (which canonicalizes sequential index types to
      // intptr_t).  Just avoid transforming this until the input has been
      // normalized.
      if (SO1->getType() != GO1->getType())
        return nullptr;
      // Only do the combine when GO1 and SO1 are both constants. Only in
      // this case, we are sure the cost after the merge is never more than
      // that before the merge.
      if (!isa<Constant>(GO1) || !isa<Constant>(SO1))
        return nullptr;
      Sum = Builder.CreateAdd(SO1, GO1);
    }

    // Update the GEP in place if possible.
    if (SrcGEP->getNumOperands() == 2) {
      GEP->setOperand(0, SrcGEP->getOperand(0));
      GEP->setOperand(1, Sum);
      return GEP;
    }
    Indices.append(SrcGEP->op_begin() + 1, SrcGEP->op_end() - 1);
    Indices.push_back(Sum);
    Indices.append(GEP->op_begin() + 2, GEP->op_end());
  } else if (isa<Constant>(*GEP->idx_begin()) &&
             cast<Constant>(*GEP->idx_begin())->isNullValue() &&
             SrcGEP->getNumOperands() != 1) {
    // Otherwise we can do the fold if the first index of the GEP is a zero
    Indices.append(SrcGEP->op_begin() + 1, SrcGEP->op_end());
    Indices.append(GEP->idx_begin() + 1, GEP->idx_end());
  }
  if (!Indices.empty())
    return Builder.CreateInBoundsGEP(SrcGEP->getSourceElementType(),
                                     SrcGEP->getOperand(0), Indices,
                                     GEP->getName());
  else
    llvm_unreachable("must merge");
}

static void MergeGepUse(Value *V) {
  for (auto U = V->user_begin(); U != V->user_end();) {
    auto Use = U++;

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(*Use)) {
      if (GEPOperator *prevGEP = dyn_cast<GEPOperator>(V)) {
        // merge the 2 GEPs
        Value *newGEP = MergeGEP(prevGEP, GEP);
        GEP->replaceAllUsesWith(newGEP);
        GEP->eraseFromParent();
        MergeGepUse(newGEP);
      } else {
        MergeGepUse(*Use);
      }
    }
    else if (GEPOperator *GEPOp = dyn_cast<GEPOperator>(*Use)) {
      if (GEPOperator *prevGEP = dyn_cast<GEPOperator>(V)) {
        // merge the 2 GEPs
        Value *newGEP = MergeGEP(prevGEP, GEP);
        GEP->replaceAllUsesWith(newGEP);
        GEP->eraseFromParent();
        MergeGepUse(newGEP);
      } else {
        MergeGepUse(*Use);
      }
    }
  }
  if (V->user_empty()) {
    if (Instruction *I = dyn_cast<Instruction>(V))
      I->eraseFromParent();
  }
}

// performScalarRepl - This algorithm is a simple worklist driven algorithm,
// which runs on all of the alloca instructions in the entry block, removing
// them if they are only used by getelementptr instructions.
//
bool SROA_HLSL::performScalarRepl(Function &F) {
  std::vector<AllocaInst *> AllocaList;
  const DataLayout &DL = F.getParent()->getDataLayout();
  Module *M = F.getParent();
  HLModule &HM = M->GetOrCreateHLModule();
  DxilTypeSystem &typeSys = HM.GetTypeSystem();

  // Scan the entry basic block, adding allocas to the worklist.
  BasicBlock &BB = F.getEntryBlock();
  for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I)
    if (AllocaInst *A = dyn_cast<AllocaInst>(I)) {
      if (A->hasNUsesOrMore(1))
        AllocaList.emplace_back(A);
    }

  // merge GEP use for the allocs
  for (auto A : AllocaList)
    MergeGepUse(A);

  DIBuilder DIB(*F.getParent(), /*AllowUnresolved*/ false);

  // Process the worklist
  bool Changed = false;
  for (AllocaInst *Alloc : AllocaList) {
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(Alloc);
    unsigned debugOffset = 0;
    std::deque<AllocaInst *> WorkList;
    WorkList.emplace_back(Alloc);
    while (!WorkList.empty()) {
      AllocaInst *AI = WorkList.front();
      WorkList.pop_front();

      // Handle dead allocas trivially.  These can be formed by SROA'ing arrays
      // with unused elements.
      if (AI->use_empty()) {
        AI->eraseFromParent();
        Changed = true;
        continue;
      }

      // If this alloca is impossible for us to promote, reject it early.
      if (AI->isArrayAllocation() || !AI->getAllocatedType()->isSized())
        continue;

      // Check to see if we can perform the core SROA transformation.  We cannot
      // transform the allocation instruction if it is an array allocation
      // (allocations OF arrays are ok though), and an allocation of a scalar
      // value cannot be decomposed at all.
      uint64_t AllocaSize = DL.getTypeAllocSize(AI->getAllocatedType());

      // Do not promote [0 x %struct].
      if (AllocaSize == 0)
        continue;

      Type *Ty = AI->getAllocatedType();
      // Skip empty struct type.
      if (SROA_Helper::IsEmptyStructType(Ty, typeSys)) {
        SROA_Helper::MarkEmptyStructUsers(AI, DeadInsts);
        DeleteDeadInstructions();
        continue;
      }

      // If the alloca looks like a good candidate for scalar replacement, and
      // if
      // all its users can be transformed, then split up the aggregate into its
      // separate elements.
      if (ShouldAttemptScalarRepl(AI) && isSafeAllocaToScalarRepl(AI)) {
        std::vector<Value *> Elts;
        IRBuilder<> Builder(AI);
        bool hasPrecise = HLModule::HasPreciseAttributeWithMetadata(AI);

        bool SROAed = SROA_Helper::DoScalarReplacement(
            AI, Elts, Builder, /*bFlatVector*/ true, hasPrecise, typeSys,
            DeadInsts);

        if (SROAed) {
          Type *Ty = AI->getAllocatedType();
          // Skip empty struct parameters.
          if (StructType *ST = dyn_cast<StructType>(Ty)) {
            if (!HLMatrixLower::IsMatrixType(Ty)) {
              DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ST);
              if (SA && SA->IsEmptyStruct()) {
                for (User *U : AI->users()) {
                  if (StoreInst *SI = dyn_cast<StoreInst>(U))
                    DeadInsts.emplace_back(SI);
                }
                DeleteDeadInstructions();
                AI->replaceAllUsesWith(UndefValue::get(AI->getType()));
                AI->eraseFromParent();
                continue;
              }
            }
          }

          // Push Elts into workList.
          for (auto iter = Elts.begin(); iter != Elts.end(); iter++)
            WorkList.emplace_back(cast<AllocaInst>(*iter));

          // Now erase any instructions that were made dead while rewriting the
          // alloca.
          DeleteDeadInstructions();
          ++NumReplaced;
          AI->eraseFromParent();
          Changed = true;
          continue;
        }
      }

      // Add debug info.
      if (DDI != nullptr && AI != Alloc) {
        Type *Ty = AI->getAllocatedType();
        unsigned size = DL.getTypeAllocSize(Ty);
        DIExpression *DDIExp = DIB.createBitPieceExpression(debugOffset, size);
        debugOffset += size;
        DIB.insertDeclare(AI, DDI->getVariable(), DDIExp, DDI->getDebugLoc(),
                          DDI);
      }
    }
  }

  return Changed;
}

// markPrecise - To save the precise attribute on alloca inst which might be removed by promote,
// mark precise attribute with function call on alloca inst stores.
bool SROA_HLSL::markPrecise(Function &F) {
  bool Changed = false;
  BasicBlock &BB = F.getEntryBlock();
  for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I)
    if (AllocaInst *A = dyn_cast<AllocaInst>(I)) {
      // TODO: Only do this on basic types.
      if (HLModule::HasPreciseAttributeWithMetadata(A)) {
        HLModule::MarkPreciseAttributeOnPtrWithFunctionCall(A,
                                                            *(F.getParent()));
        Changed = true;
      }
    }
  return Changed;
}

/// DeleteDeadInstructions - Erase instructions on the DeadInstrs list,
/// recursively including all their operands that become trivially dead.
void SROA_HLSL::DeleteDeadInstructions() {
  while (!DeadInsts.empty()) {
    Instruction *I = cast<Instruction>(DeadInsts.pop_back_val());

    for (User::op_iterator OI = I->op_begin(), E = I->op_end(); OI != E; ++OI)
      if (Instruction *U = dyn_cast<Instruction>(*OI)) {
        // Zero out the operand and see if it becomes trivially dead.
        // (But, don't add allocas to the dead instruction list -- they are
        // already on the worklist and will be deleted separately.)
        *OI = nullptr;
        if (isInstructionTriviallyDead(U) && !isa<AllocaInst>(U))
          DeadInsts.push_back(U);
      }

    I->eraseFromParent();
  }
}

/// isSafeForScalarRepl - Check if instruction I is a safe use with regard to
/// performing scalar replacement of alloca AI.  The results are flagged in
/// the Info parameter.  Offset indicates the position within AI that is
/// referenced by this instruction.
void SROA_HLSL::isSafeForScalarRepl(Instruction *I, uint64_t Offset,
                                    AllocaInfo &Info) {
  if (I->getType()->isPointerTy()) {
    // Don't check object pointers.
    if (HLModule::IsHLSLObjectType(I->getType()->getPointerElementType()))
      return;
  }
  const DataLayout &DL = I->getModule()->getDataLayout();
  for (Use &U : I->uses()) {
    Instruction *User = cast<Instruction>(U.getUser());

    if (BitCastInst *BC = dyn_cast<BitCastInst>(User)) {
      isSafeForScalarRepl(BC, Offset, Info);
    } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(User)) {
      uint64_t GEPOffset = Offset;
      isSafeGEP(GEPI, GEPOffset, Info);
      if (!Info.isUnsafe)
        isSafeForScalarRepl(GEPI, GEPOffset, Info);
    } else if (MemIntrinsic *MI = dyn_cast<MemIntrinsic>(User)) {
      ConstantInt *Length = dyn_cast<ConstantInt>(MI->getLength());
      if (!Length || Length->isNegative())
        return MarkUnsafe(Info, User);

      isSafeMemAccess(Offset, Length->getZExtValue(), nullptr,
                      U.getOperandNo() == 0, Info, MI,
                      true /*AllowWholeAccess*/);
    } else if (LoadInst *LI = dyn_cast<LoadInst>(User)) {
      if (!LI->isSimple())
        return MarkUnsafe(Info, User);
      Type *LIType = LI->getType();
      isSafeMemAccess(Offset, DL.getTypeAllocSize(LIType), LIType, false, Info,
                      LI, true /*AllowWholeAccess*/);
      Info.hasALoadOrStore = true;

    } else if (StoreInst *SI = dyn_cast<StoreInst>(User)) {
      // Store is ok if storing INTO the pointer, not storing the pointer
      if (!SI->isSimple() || SI->getOperand(0) == I)
        return MarkUnsafe(Info, User);

      Type *SIType = SI->getOperand(0)->getType();
      isSafeMemAccess(Offset, DL.getTypeAllocSize(SIType), SIType, true, Info,
                      SI, true /*AllowWholeAccess*/);
      Info.hasALoadOrStore = true;
    } else if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(User)) {
      if (II->getIntrinsicID() != Intrinsic::lifetime_start &&
          II->getIntrinsicID() != Intrinsic::lifetime_end)
        return MarkUnsafe(Info, User);
    } else if (isa<PHINode>(User) || isa<SelectInst>(User)) {
      isSafePHISelectUseForScalarRepl(User, Offset, Info);
    } else if (CallInst *CI = dyn_cast<CallInst>(User)) {
      HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
      // HL functions are safe for scalar repl.
      if (group == HLOpcodeGroup::NotHL)
        return MarkUnsafe(Info, User);
    } else {
      return MarkUnsafe(Info, User);
    }
    if (Info.isUnsafe)
      return;
  }
}

/// isSafePHIUseForScalarRepl - If we see a PHI node or select using a pointer
/// derived from the alloca, we can often still split the alloca into elements.
/// This is useful if we have a large alloca where one element is phi'd
/// together somewhere: we can SRoA and promote all the other elements even if
/// we end up not being able to promote this one.
///
/// All we require is that the uses of the PHI do not index into other parts of
/// the alloca.  The most important use case for this is single load and stores
/// that are PHI'd together, which can happen due to code sinking.
void SROA_HLSL::isSafePHISelectUseForScalarRepl(Instruction *I, uint64_t Offset,
                                                AllocaInfo &Info) {
  // If we've already checked this PHI, don't do it again.
  if (PHINode *PN = dyn_cast<PHINode>(I))
    if (!Info.CheckedPHIs.insert(PN).second)
      return;

  const DataLayout &DL = I->getModule()->getDataLayout();
  for (User *U : I->users()) {
    Instruction *UI = cast<Instruction>(U);

    if (BitCastInst *BC = dyn_cast<BitCastInst>(UI)) {
      isSafePHISelectUseForScalarRepl(BC, Offset, Info);
    } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(UI)) {
      // Only allow "bitcast" GEPs for simplicity.  We could generalize this,
      // but would have to prove that we're staying inside of an element being
      // promoted.
      if (!GEPI->hasAllZeroIndices())
        return MarkUnsafe(Info, UI);
      isSafePHISelectUseForScalarRepl(GEPI, Offset, Info);
    } else if (LoadInst *LI = dyn_cast<LoadInst>(UI)) {
      if (!LI->isSimple())
        return MarkUnsafe(Info, UI);
      Type *LIType = LI->getType();
      isSafeMemAccess(Offset, DL.getTypeAllocSize(LIType), LIType, false, Info,
                      LI, false /*AllowWholeAccess*/);
      Info.hasALoadOrStore = true;

    } else if (StoreInst *SI = dyn_cast<StoreInst>(UI)) {
      // Store is ok if storing INTO the pointer, not storing the pointer
      if (!SI->isSimple() || SI->getOperand(0) == I)
        return MarkUnsafe(Info, UI);

      Type *SIType = SI->getOperand(0)->getType();
      isSafeMemAccess(Offset, DL.getTypeAllocSize(SIType), SIType, true, Info,
                      SI, false /*AllowWholeAccess*/);
      Info.hasALoadOrStore = true;
    } else if (isa<PHINode>(UI) || isa<SelectInst>(UI)) {
      isSafePHISelectUseForScalarRepl(UI, Offset, Info);
    } else {
      return MarkUnsafe(Info, UI);
    }
    if (Info.isUnsafe)
      return;
  }
}

/// isSafeGEP - Check if a GEP instruction can be handled for scalar
/// replacement.  It is safe when all the indices are constant, in-bounds
/// references, and when the resulting offset corresponds to an element within
/// the alloca type.  The results are flagged in the Info parameter.  Upon
/// return, Offset is adjusted as specified by the GEP indices.
void SROA_HLSL::isSafeGEP(GetElementPtrInst *GEPI, uint64_t &Offset,
                          AllocaInfo &Info) {
  gep_type_iterator GEPIt = gep_type_begin(GEPI), E = gep_type_end(GEPI);
  if (GEPIt == E)
    return;
  bool NonConstant = false;
  unsigned NonConstantIdxSize = 0;

  // Compute the offset due to this GEP and check if the alloca has a
  // component element at that offset.
  SmallVector<Value *, 8> Indices(GEPI->op_begin() + 1, GEPI->op_end());
  auto indicesIt = Indices.begin();

  // Walk through the GEP type indices, checking the types that this indexes
  // into.
  uint32_t arraySize = 0;
  bool isArrayIndexing = false;

  for (;GEPIt != E; ++GEPIt) {
    Type *Ty = *GEPIt;
    if (Ty->isStructTy() && !HLMatrixLower::IsMatrixType(Ty)) {
      // Don't go inside struct when mark hasArrayIndexing and hasVectorIndexing.
      // The following level won't affect scalar repl on the struct.
      break;
    }
    if (GEPIt->isArrayTy()) {
      arraySize = GEPIt->getArrayNumElements();
      isArrayIndexing = true;
    }
    if (GEPIt->isVectorTy()) {
      arraySize = GEPIt->getVectorNumElements();
      isArrayIndexing = false;
    }
    // Allow dynamic indexing
    ConstantInt *IdxVal = dyn_cast<ConstantInt>(GEPIt.getOperand());
    if (!IdxVal) {
      // for dynamic index, use array size - 1 to check the offset
      *indicesIt = Constant::getIntegerValue(
          Type::getInt32Ty(GEPI->getContext()), APInt(32, arraySize - 1));
      if (isArrayIndexing)
        Info.hasArrayIndexing = true;
      else
        Info.hasVectorIndexing = true;
      NonConstant = true;
    }
    indicesIt++;
  }
  // Continue iterate only for the NonConstant.
  for (;GEPIt != E; ++GEPIt) {
    Type *Ty = *GEPIt;
    if (Ty->isArrayTy()) {
      arraySize = GEPIt->getArrayNumElements();
    }
    if (Ty->isVectorTy()) {
      arraySize = GEPIt->getVectorNumElements();
    }
    // Allow dynamic indexing
    ConstantInt *IdxVal = dyn_cast<ConstantInt>(GEPIt.getOperand());
    if (!IdxVal) {
      // for dynamic index, use array size - 1 to check the offset
      *indicesIt = Constant::getIntegerValue(
          Type::getInt32Ty(GEPI->getContext()), APInt(32, arraySize - 1));
      NonConstant = true;
    }
    indicesIt++;
  }
  // If this GEP is non-constant then the last operand must have been a
  // dynamic index into a vector.  Pop this now as it has no impact on the
  // constant part of the offset.
  if (NonConstant)
    Indices.pop_back();

  const DataLayout &DL = GEPI->getModule()->getDataLayout();
  Offset += DL.getIndexedOffset(GEPI->getPointerOperandType(), Indices);
  if (!TypeHasComponent(Info.AI->getAllocatedType(), Offset, NonConstantIdxSize,
                        DL))
    MarkUnsafe(Info, GEPI);
}

/// isHomogeneousAggregate - Check if type T is a struct or array containing
/// elements of the same type (which is always true for arrays).  If so,
/// return true with NumElts and EltTy set to the number of elements and the
/// element type, respectively.
static bool isHomogeneousAggregate(Type *T, unsigned &NumElts, Type *&EltTy) {
  if (ArrayType *AT = dyn_cast<ArrayType>(T)) {
    NumElts = AT->getNumElements();
    EltTy = (NumElts == 0 ? nullptr : AT->getElementType());
    return true;
  }
  if (StructType *ST = dyn_cast<StructType>(T)) {
    NumElts = ST->getNumContainedTypes();
    EltTy = (NumElts == 0 ? nullptr : ST->getContainedType(0));
    for (unsigned n = 1; n < NumElts; ++n) {
      if (ST->getContainedType(n) != EltTy)
        return false;
    }
    return true;
  }
  return false;
}

/// isCompatibleAggregate - Check if T1 and T2 are either the same type or are
/// "homogeneous" aggregates with the same element type and number of elements.
static bool isCompatibleAggregate(Type *T1, Type *T2) {
  if (T1 == T2)
    return true;

  unsigned NumElts1, NumElts2;
  Type *EltTy1, *EltTy2;
  if (isHomogeneousAggregate(T1, NumElts1, EltTy1) &&
      isHomogeneousAggregate(T2, NumElts2, EltTy2) && NumElts1 == NumElts2 &&
      EltTy1 == EltTy2)
    return true;

  return false;
}

/// isSafeMemAccess - Check if a load/store/memcpy operates on the entire AI
/// alloca or has an offset and size that corresponds to a component element
/// within it.  The offset checked here may have been formed from a GEP with a
/// pointer bitcasted to a different type.
///
/// If AllowWholeAccess is true, then this allows uses of the entire alloca as a
/// unit.  If false, it only allows accesses known to be in a single element.
void SROA_HLSL::isSafeMemAccess(uint64_t Offset, uint64_t MemSize,
                                Type *MemOpType, bool isStore, AllocaInfo &Info,
                                Instruction *TheAccess, bool AllowWholeAccess) {
  // What hlsl cares is Info.hasVectorIndexing.
  // Do nothing here.
}

/// TypeHasComponent - Return true if T has a component type with the
/// specified offset and size.  If Size is zero, do not check the size.
bool SROA_HLSL::TypeHasComponent(Type *T, uint64_t Offset, uint64_t Size,
                                 const DataLayout &DL) {
  Type *EltTy;
  uint64_t EltSize;
  if (StructType *ST = dyn_cast<StructType>(T)) {
    const StructLayout *Layout = DL.getStructLayout(ST);
    unsigned EltIdx = Layout->getElementContainingOffset(Offset);
    EltTy = ST->getContainedType(EltIdx);
    EltSize = DL.getTypeAllocSize(EltTy);
    Offset -= Layout->getElementOffset(EltIdx);
  } else if (ArrayType *AT = dyn_cast<ArrayType>(T)) {
    EltTy = AT->getElementType();
    EltSize = DL.getTypeAllocSize(EltTy);
    if (Offset >= AT->getNumElements() * EltSize)
      return false;
    Offset %= EltSize;
  } else if (VectorType *VT = dyn_cast<VectorType>(T)) {
    EltTy = VT->getElementType();
    EltSize = DL.getTypeAllocSize(EltTy);
    if (Offset >= VT->getNumElements() * EltSize)
      return false;
    Offset %= EltSize;
  } else {
    return false;
  }
  if (Offset == 0 && (Size == 0 || EltSize == Size))
    return true;
  // Check if the component spans multiple elements.
  if (Offset + Size > EltSize)
    return false;
  return TypeHasComponent(EltTy, Offset, Size, DL);
}

/// LoadVectorArray - Load vector array like [2 x <4 x float>] from
///  arrays like 4 [2 x float].
static Value *LoadVectorArray(ArrayType *AT, ArrayRef<Value *> NewElts,
                              SmallVector<Value *, 8> &idxList,
                              IRBuilder<> &Builder) {
  Type *EltTy = AT->getElementType();
  Value *retVal = llvm::UndefValue::get(AT);
  Type *i32Ty = Type::getInt32Ty(EltTy->getContext());

  uint32_t arraySize = AT->getNumElements();
  for (uint32_t i = 0; i < arraySize; i++) {
    Constant *idx = ConstantInt::get(i32Ty, i);
    idxList.emplace_back(idx);

    if (ArrayType *EltAT = dyn_cast<ArrayType>(EltTy)) {
      Value *EltVal = LoadVectorArray(EltAT, NewElts, idxList, Builder);
      retVal = Builder.CreateInsertValue(retVal, EltVal, i);
    } else {
      assert(EltTy->isVectorTy() ||
             EltTy->isStructTy() && "must be a vector or struct type");
      bool isVectorTy = EltTy->isVectorTy();
      Value *retVec = llvm::UndefValue::get(EltTy);

      if (isVectorTy) {
        for (uint32_t c = 0; c < EltTy->getVectorNumElements(); c++) {
          Value *GEP = Builder.CreateInBoundsGEP(NewElts[c], idxList);
          Value *elt = Builder.CreateLoad(GEP);
          retVec = Builder.CreateInsertElement(retVec, elt, c);
        }
      } else {
        for (uint32_t c = 0; c < EltTy->getStructNumElements(); c++) {
          Value *GEP = Builder.CreateInBoundsGEP(NewElts[c], idxList);
          Value *elt = Builder.CreateLoad(GEP);
          retVec = Builder.CreateInsertValue(retVec, elt, c);
        }
      }

      retVal = Builder.CreateInsertValue(retVal, retVec, i);
    }
    idxList.pop_back();
  }
  return retVal;
}
/// LoadVectorArray - Store vector array like [2 x <4 x float>] to
///  arrays like 4 [2 x float].
static void StoreVectorArray(ArrayType *AT, Value *val,
                             ArrayRef<Value *> NewElts,
                             SmallVector<Value *, 8> &idxList,
                             IRBuilder<> &Builder) {
  Type *EltTy = AT->getElementType();
  Type *i32Ty = Type::getInt32Ty(EltTy->getContext());

  uint32_t arraySize = AT->getNumElements();
  for (uint32_t i = 0; i < arraySize; i++) {
    Value *elt = Builder.CreateExtractValue(val, i);

    Constant *idx = ConstantInt::get(i32Ty, i);
    idxList.emplace_back(idx);

    if (ArrayType *EltAT = dyn_cast<ArrayType>(EltTy)) {
      StoreVectorArray(EltAT, elt, NewElts, idxList, Builder);
    } else {
      assert(EltTy->isVectorTy() ||
             EltTy->isStructTy() && "must be a vector or struct type");
      bool isVectorTy = EltTy->isVectorTy();
      if (isVectorTy) {
        for (uint32_t c = 0; c < EltTy->getVectorNumElements(); c++) {
          Value *component = Builder.CreateExtractElement(elt, c);
          Value *GEP = Builder.CreateInBoundsGEP(NewElts[c], idxList);
          Builder.CreateStore(component, GEP);
        }
      } else {
        for (uint32_t c = 0; c < EltTy->getStructNumElements(); c++) {
          Value *field = Builder.CreateExtractValue(elt, c);
          Value *GEP = Builder.CreateInBoundsGEP(NewElts[c], idxList);
          Builder.CreateStore(field, GEP);
        }
      }
    }
    idxList.pop_back();
  }
}

/// HasPadding - Return true if the specified type has any structure or
/// alignment padding in between the elements that would be split apart
/// by SROA; return false otherwise.
static bool HasPadding(Type *Ty, const DataLayout &DL) {
  if (ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    Ty = ATy->getElementType();
    return DL.getTypeSizeInBits(Ty) != DL.getTypeAllocSizeInBits(Ty);
  }

  // SROA currently handles only Arrays and Structs.
  StructType *STy = cast<StructType>(Ty);
  const StructLayout *SL = DL.getStructLayout(STy);
  unsigned PrevFieldBitOffset = 0;
  for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
    unsigned FieldBitOffset = SL->getElementOffsetInBits(i);

    // Check to see if there is any padding between this element and the
    // previous one.
    if (i) {
      unsigned PrevFieldEnd =
          PrevFieldBitOffset + DL.getTypeSizeInBits(STy->getElementType(i - 1));
      if (PrevFieldEnd < FieldBitOffset)
        return true;
    }
    PrevFieldBitOffset = FieldBitOffset;
  }
  // Check for tail padding.
  if (unsigned EltCount = STy->getNumElements()) {
    unsigned PrevFieldEnd =
        PrevFieldBitOffset +
        DL.getTypeSizeInBits(STy->getElementType(EltCount - 1));
    if (PrevFieldEnd < SL->getSizeInBits())
      return true;
  }
  return false;
}

/// isSafeStructAllocaToScalarRepl - Check to see if the specified allocation of
/// an aggregate can be broken down into elements.  Return 0 if not, 3 if safe,
/// or 1 if safe after canonicalization has been performed.
bool SROA_HLSL::isSafeAllocaToScalarRepl(AllocaInst *AI) {
  // Loop over the use list of the alloca.  We can only transform it if all of
  // the users are safe to transform.
  AllocaInfo Info(AI);

  isSafeForScalarRepl(AI, 0, Info);
  if (Info.isUnsafe) {
    DEBUG(dbgs() << "Cannot transform: " << *AI << '\n');
    return false;
  }

  // vector indexing need translate vector into array
  if (Info.hasVectorIndexing)
    return false;

  const DataLayout &DL = AI->getModule()->getDataLayout();

  // Okay, we know all the users are promotable.  If the aggregate is a memcpy
  // source and destination, we have to be careful.  In particular, the memcpy
  // could be moving around elements that live in structure padding of the LLVM
  // types, but may actually be used.  In these cases, we refuse to promote the
  // struct.
  if (Info.isMemCpySrc && Info.isMemCpyDst &&
      HasPadding(AI->getAllocatedType(), DL))
    return false;

  return true;
}

// Copy data from srcPtr to destPtr.
static void SimplePtrCopy(Value *DestPtr, Value *SrcPtr,
                       llvm::SmallVector<llvm::Value *, 16> &idxList,
                       bool bAllowReplace,
                       IRBuilder<> &Builder) {
  // If src only has one use, just replace Dest with Src.
  if (bAllowReplace && SrcPtr->hasOneUse() &&
        // Only 2 uses for dest: 1 for memcpy, 1 for other use.
      !DestPtr->hasNUsesOrMore(3) &&
      !isa<CallInst>(SrcPtr) && !isa<CallInst>(DestPtr)) {
    DestPtr->replaceAllUsesWith(SrcPtr);
  } else {
    if (idxList.size() > 1) {
      DestPtr = Builder.CreateInBoundsGEP(DestPtr, idxList);
      SrcPtr = Builder.CreateInBoundsGEP(SrcPtr, idxList);
    }
    llvm::LoadInst *ld = Builder.CreateLoad(SrcPtr);
    Builder.CreateStore(ld, DestPtr);
  }
}

// Copy srcVal to destPtr.
static void SimpleValCopy(Value *DestPtr, Value *SrcVal,
                       llvm::SmallVector<llvm::Value *, 16> &idxList,
                       IRBuilder<> &Builder) {
  Value *DestGEP = Builder.CreateInBoundsGEP(DestPtr, idxList);
  Value *Val = SrcVal;
  // Skip beginning pointer type.
  for (unsigned i = 1; i < idxList.size(); i++) {
    ConstantInt *idx = cast<ConstantInt>(idxList[i]);
    Type *Ty = Val->getType();
    if (Ty->isAggregateType()) {
      Val = Builder.CreateExtractValue(Val, idx->getLimitedValue());
    }
  }

  Builder.CreateStore(Val, DestGEP);
}

static void SimpleCopy(Value *Dest, Value *Src,
                       llvm::SmallVector<llvm::Value *, 16> &idxList,
                       bool bAllowReplace,
                       IRBuilder<> &Builder) {
  if (Src->getType()->isPointerTy())
    SimplePtrCopy(Dest, Src, idxList, bAllowReplace, Builder);
  else
    SimpleValCopy(Dest, Src, idxList, Builder);
}
// Split copy into ld/st.
static void SplitCpy(Type *Ty, Value *Dest, Value *Src,
                     SmallVector<Value *, 16> &idxList,
                     bool bAllowReplace,
                     IRBuilder<> &Builder) {
  if (PointerType *PT = dyn_cast<PointerType>(Ty)) {
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Ty->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    SplitCpy(PT->getElementType(), Dest, Src, idxList, bAllowReplace, Builder);

    idxList.pop_back();
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    Module *M = Builder.GetInsertPoint()->getModule();
    Value *DestGEP = Builder.CreateInBoundsGEP(Dest, idxList);
    Value *SrcGEP = Builder.CreateInBoundsGEP(Src, idxList);

    Value *Load = HLModule::EmitHLOperationCall(
        Builder, HLOpcodeGroup::HLMatLoadStore,
        static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatLoad), Ty, {SrcGEP},
        *M);

    // Generate Matrix Store.
    HLModule::EmitHLOperationCall(
        Builder, HLOpcodeGroup::HLMatLoadStore,
        static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatStore), Ty,
        {DestGEP, Load}, *M);

  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (HLModule::IsHLSLObjectType(ST)) {
      // Avoid split HLSL object.
      SimpleCopy(Dest, Src, idxList, bAllowReplace, Builder);
      return;
    }
    for (uint32_t i = 0; i < ST->getNumElements(); i++) {
      llvm::Type *ET = ST->getElementType(i);

      Constant *idx = llvm::Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      SplitCpy(ET, Dest, Src, idxList, bAllowReplace, Builder);

      idxList.pop_back();
    }

  } else if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
    Type *ET = AT->getElementType();

    for (uint32_t i = 0; i < AT->getNumElements(); i++) {
      Constant *idx = Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);
      SplitCpy(ET, Dest, Src, idxList, bAllowReplace, Builder);

      idxList.pop_back();
    }
  } else {
    SimpleCopy(Dest, Src, idxList, bAllowReplace, Builder);
  }
}

static void SplitPtr(Type *Ty, Value *Ptr, SmallVector<Value *, 16> &idxList,
                     SmallVector<Value *, 16> &EltPtrList,
                     IRBuilder<> &Builder) {
  if (PointerType *PT = dyn_cast<PointerType>(Ty)) {
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Ty->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    SplitPtr(PT->getElementType(), Ptr, idxList, EltPtrList, Builder);

    idxList.pop_back();
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    Value *GEP = Builder.CreateInBoundsGEP(Ptr, idxList);
    EltPtrList.emplace_back(GEP);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (HLModule::IsHLSLObjectType(ST)) {
      // Avoid split HLSL object.
      Value *GEP = Builder.CreateInBoundsGEP(Ptr, idxList);
      EltPtrList.emplace_back(GEP);
      return;
    }
    for (uint32_t i = 0; i < ST->getNumElements(); i++) {
      llvm::Type *ET = ST->getElementType(i);

      Constant *idx = llvm::Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      SplitPtr(ET, Ptr, idxList, EltPtrList, Builder);

      idxList.pop_back();
    }

  } else if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
    if (AT->getNumContainedTypes() == 0) {
      // Skip case like [0 x %struct].
      return;
    }
    Type *ElTy = AT->getElementType();
    SmallVector<ArrayType *, 4> nestArrayTys;

    nestArrayTys.emplace_back(AT);
    // support multi level of array
    while (ElTy->isArrayTy()) {
      ArrayType *ElAT = cast<ArrayType>(ElTy);
      nestArrayTys.emplace_back(ElAT);
      ElTy = ElAT->getElementType();
    }

    if (!ElTy->isStructTy() ||
        HLMatrixLower::IsMatrixType(ElTy)) {
      // Not split array of basic type.
      Value *GEP = Builder.CreateInBoundsGEP(Ptr, idxList);
      EltPtrList.emplace_back(GEP);
    }
    else {
      DXASSERT(0, "Not support array of struct when split pointers.");
    }
  } else {
    Value *GEP = Builder.CreateInBoundsGEP(Ptr, idxList);
    EltPtrList.emplace_back(GEP);
  }
}

void MemcpySplitter::Split(llvm::Function &F) {
  // Walk all instruction in the function.
  for (Function::iterator BB = F.begin(), BBE = F.end(); BB != BBE; ++BB) {
    for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE;) {
      // Avoid invalidating the iterator.
      Instruction *I = BI++;

      if (MemCpyInst *MI = dyn_cast<MemCpyInst>(I)) {
        Value *Op0 = MI->getOperand(0);
        Value *Op1 = MI->getOperand(1);

        Value *Dest = MI->getRawDest();
        Value *Src = MI->getRawSource();
        // Only remove one level bitcast generated from inline.
        if (Operator::getOpcode(Dest) == Instruction::BitCast)
          Dest = cast<Operator>(Dest)->getOperand(0);
        if (Operator::getOpcode(Src) == Instruction::BitCast)
          Src = cast<Operator>(Src)->getOperand(0);

        IRBuilder<> Builder(I);
        if (Dest->getType() != Src->getType()) {
          // Support case when bitcast (gep ptr, 0,0) is transformed into
          // bitcast ptr.
          if (isa<GlobalVariable>(Src) &&
              Src->getType()->getPointerElementType()->isStructTy()) {
            StructType *ST =
                cast<StructType>(Src->getType()->getPointerElementType());

            if (Dest->getType()->getPointerElementType() ==
                ST->getElementType(0)) {
              Type *i32Ty = Type::getInt32Ty(F.getContext());
              Value *zero = ConstantInt::get(i32Ty, 0);
              Src = Builder.CreateInBoundsGEP(Src, {zero, zero});
            }
          }

          if (Dest->getType() != Src->getType())
            continue;
        }
        llvm::SmallVector<llvm::Value *, 16> idxList;
        // split
        SplitCpy(Dest->getType(), Dest, Src, idxList, /*bAllowReplace*/true, Builder);
        // delete memcpy
        I->eraseFromParent();
        if (Instruction *op0 = dyn_cast<Instruction>(Op0)) {
          if (op0->user_empty())
            op0->eraseFromParent();
        }
        if (Instruction *op1 = dyn_cast<Instruction>(Op1)) {
          if (op1->user_empty())
            op1->eraseFromParent();
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// SRoA Helper
//===----------------------------------------------------------------------===//

/// RewriteGEP - Rewrite the GEP to be relative to new element when can find a
/// new element which is struct field. If cannot find, create new element GEPs
/// and try to rewrite GEP with new GEPS.
void SROA_Helper::RewriteForGEP(GEPOperator *GEP, IRBuilder<> &Builder) {

  assert(OldVal == GEP->getPointerOperand() && "");

  Value *NewPointer = nullptr;
  SmallVector<Value *, 8> NewArgs;

  gep_type_iterator GEPIt = gep_type_begin(GEP), E = gep_type_end(GEP);
  for (; GEPIt != E; ++GEPIt) {
    if (GEPIt->isStructTy()) {
      // must be const
      ConstantInt *IdxVal = dyn_cast<ConstantInt>(GEPIt.getOperand());
      assert(IdxVal->getLimitedValue() < NewElts.size() && "");
      NewPointer = NewElts[IdxVal->getLimitedValue()];
      // The idx is used for NewPointer, not part of newGEP idx,
      GEPIt++;
      break;
    } else if (GEPIt->isArrayTy()) {
      // Add array idx.
      NewArgs.push_back(GEPIt.getOperand());
    } else if (GEPIt->isPointerTy()) {
      // Add pointer idx.
      NewArgs.push_back(GEPIt.getOperand());
    } else if (GEPIt->isVectorTy()) {
      // Add vector idx.
      NewArgs.push_back(GEPIt.getOperand());
    } else {
      llvm_unreachable("should break from structTy");
    }
  }

  if (NewPointer) {
    // Struct split.
    // Add rest of idx.
    for (; GEPIt != E; ++GEPIt) {
      NewArgs.push_back(GEPIt.getOperand());
    }
    // If only 1 level struct, just use the new pointer.
    Value *NewGEP = NewPointer;
    if (NewArgs.size() > 1) {
      NewGEP = Builder.CreateInBoundsGEP(NewPointer, NewArgs);
      NewGEP->takeName(GEP);
    }

    assert(NewGEP->getType() == GEP->getType() && "type mismatch");
    
    GEP->replaceAllUsesWith(NewGEP);
    if (isa<Instruction>(GEP))
      DeadInsts.push_back(GEP);
  } else {
    // End at array of basic type.
    Type *Ty = GEP->getType()->getPointerElementType();
    if (Ty->isVectorTy() || Ty->isStructTy() || Ty->isArrayTy()) {
      SmallVector<Value *, 8> NewArgs;
      NewArgs.append(GEP->idx_begin(), GEP->idx_end());

      SmallVector<Value *, 8> NewGEPs;
      // create new geps
      for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
        Value *NewGEP = Builder.CreateGEP(nullptr, NewElts[i], NewArgs);
        NewGEPs.emplace_back(NewGEP);
      }
      SROA_Helper helper(GEP, NewGEPs, DeadInsts);
      helper.RewriteForScalarRepl(GEP, Builder);
      for (Value *NewGEP : NewGEPs) {
        if (NewGEP->user_empty() && isa<Instruction>(NewGEP)) {
          // Delete unused newGEP.
          DeadInsts.emplace_back(NewGEP);
        }
      }
    } else {
      Value *vecIdx = NewArgs.back();
      if (ConstantInt *immVecIdx = dyn_cast<ConstantInt>(vecIdx)) {
        // Replace vecArray[arrayIdx][immVecIdx]
        // with scalarArray_immVecIdx[arrayIdx]

        // Pop the vecIdx.
        NewArgs.pop_back();
        Value *NewGEP = NewElts[immVecIdx->getLimitedValue()];
        if (NewArgs.size() > 1) {
          NewGEP = Builder.CreateInBoundsGEP(NewGEP, NewArgs);
          NewGEP->takeName(GEP);
        }

        assert(NewGEP->getType() == GEP->getType() && "type mismatch");

        GEP->replaceAllUsesWith(NewGEP);
        if (isa<Instruction>(GEP))
          DeadInsts.push_back(GEP);
      } else {
        // dynamic vector indexing.
        assert(0 && "should not reach here");
      }
    }
  }
}

/// isVectorArray - Check if T is array of vector.
static bool isVectorArray(Type *T) {
  if (!T->isArrayTy())
    return false;

  while (T->getArrayElementType()->isArrayTy()) {
    T = T->getArrayElementType();
  }

  return T->getArrayElementType()->isVectorTy();
}

static void SimplifyStructValUsage(Value *StructVal, std::vector<Value *> Elts,
                                   SmallVectorImpl<Value *> &DeadInsts) {
  for (User *user : StructVal->users()) {
    if (ExtractValueInst *Extract = dyn_cast<ExtractValueInst>(user)) {
      DXASSERT(Extract->getNumIndices() == 1, "only support 1 index case");
      unsigned index = Extract->getIndices()[0];
      Value *Elt = Elts[index];
      Extract->replaceAllUsesWith(Elt);
      DeadInsts.emplace_back(Extract);
    } else if (InsertValueInst *Insert = dyn_cast<InsertValueInst>(user)) {
      DXASSERT(Insert->getNumIndices() == 1, "only support 1 index case");
      unsigned index = Insert->getIndices()[0];
      if (Insert->getAggregateOperand() == StructVal) {
        // Update field.
        std::vector<Value *> NewElts = Elts;
        NewElts[index] = Insert->getInsertedValueOperand();
        SimplifyStructValUsage(Insert, NewElts, DeadInsts);
      } else {
        // Insert to another bigger struct.
        IRBuilder<> Builder(Insert);
        Value *TmpStructVal = UndefValue::get(StructVal->getType());
        for (unsigned i = 0; i < Elts.size(); i++) {
          TmpStructVal =
              Builder.CreateInsertValue(TmpStructVal, Elts[i], {i});
        }
        Insert->replaceUsesOfWith(StructVal, TmpStructVal);
      }
    }
  }
}

/// RewriteForLoad - Replace OldVal with flattened NewElts in LoadInst.
void SROA_Helper::RewriteForLoad(LoadInst *LI) {
  Type *LIType = LI->getType();
  Type *ValTy = OldVal->getType()->getPointerElementType();
  IRBuilder<> Builder(LI);
  if (LIType->isVectorTy()) {
    // Replace:
    //   %res = load { 2 x i32 }* %alloc
    // with:
    //   %load.0 = load i32* %alloc.0
    //   %insert.0 insertvalue { 2 x i32 } zeroinitializer, i32 %load.0, 0
    //   %load.1 = load i32* %alloc.1
    //   %insert = insertvalue { 2 x i32 } %insert.0, i32 %load.1, 1
    Value *Insert = UndefValue::get(LIType);
    for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
      Value *Load = Builder.CreateLoad(NewElts[i], "load");
      Insert = Builder.CreateInsertElement(Insert, Load, i, "insert");
    }
    LI->replaceAllUsesWith(Insert);
    DeadInsts.push_back(LI);
  } else if (isCompatibleAggregate(LIType, ValTy)) {
    if (isVectorArray(LIType)) {
      // Replace:
      //   %res = load [2 x <2 x float>] * %alloc
      // with:
      //   %load.0 = load [4 x float]* %alloc.0
      //   %insert.0 insertvalue [4 x float] zeroinitializer,i32 %load.0,0
      //   %load.1 = load [4 x float]* %alloc.1
      //   %insert = insertvalue [4 x float] %insert.0, i32 %load.1, 1
      //  ...
      Type *i32Ty = Type::getInt32Ty(LIType->getContext());
      Value *zero = ConstantInt::get(i32Ty, 0);
      SmallVector<Value *, 8> idxList;
      idxList.emplace_back(zero);
      Value *newLd =
          LoadVectorArray(cast<ArrayType>(LIType), NewElts, idxList, Builder);
      LI->replaceAllUsesWith(newLd);
      DeadInsts.push_back(LI);
    } else {
      // Replace:
      //   %res = load { i32, i32 }* %alloc
      // with:
      //   %load.0 = load i32* %alloc.0
      //   %insert.0 insertvalue { i32, i32 } zeroinitializer, i32 %load.0,
      //   0
      //   %load.1 = load i32* %alloc.1
      //   %insert = insertvalue { i32, i32 } %insert.0, i32 %load.1, 1
      // (Also works for arrays instead of structs)
      Module *M = LI->getModule();
      Value *Insert = UndefValue::get(LIType);
      std::vector<Value *> LdElts(NewElts.size());
      for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
        Value *Ptr = NewElts[i];
        Type *Ty = Ptr->getType()->getPointerElementType();
        Value *Load = nullptr;
        if (!HLMatrixLower::IsMatrixType(Ty))
          Load = Builder.CreateLoad(Ptr, "load");
        else {
          // Generate Matrix Load.
          Load = HLModule::EmitHLOperationCall(
              Builder, HLOpcodeGroup::HLMatLoadStore,
              static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatLoad), Ty,
              {Ptr}, *M);
        }
        LdElts[i] = Load;
        Insert = Builder.CreateInsertValue(Insert, Load, i, "insert");
      }
      LI->replaceAllUsesWith(Insert);
      if (LIType->isStructTy()) {
        SimplifyStructValUsage(Insert, LdElts, DeadInsts);
      }
      DeadInsts.push_back(LI);
    }
  } else {
    llvm_unreachable("other type don't need rewrite");
  }
}

/// RewriteForStore - Replace OldVal with flattened NewElts in StoreInst.
void SROA_Helper::RewriteForStore(StoreInst *SI) {
  Value *Val = SI->getOperand(0);
  Type *SIType = Val->getType();
  IRBuilder<> Builder(SI);
  Type *ValTy = OldVal->getType()->getPointerElementType();
  if (SIType->isVectorTy()) {
    // Replace:
    //   store <2 x float> %val, <2 x float>* %alloc
    // with:
    //   %val.0 = extractelement { 2 x float } %val, 0
    //   store i32 %val.0, i32* %alloc.0
    //   %val.1 = extractelement { 2 x float } %val, 1
    //   store i32 %val.1, i32* %alloc.1

    for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
      Value *Extract = Builder.CreateExtractElement(Val, i, Val->getName());
      Builder.CreateStore(Extract, NewElts[i]);
    }
    DeadInsts.push_back(SI);
  } else if (isCompatibleAggregate(SIType, ValTy)) {
    if (isVectorArray(SIType)) {
      // Replace:
      //   store [2 x <2 x i32>] %val, [2 x <2 x i32>]* %alloc, align 16
      // with:
      //   %val.0 = extractvalue [2 x <2 x i32>] %val, 0
      //   %all0c.0.0 = getelementptr inbounds [2 x i32], [2 x i32]* %alloc.0,
      //   i32 0, i32 0
      //   %val.0.0 = extractelement <2 x i32> %243, i64 0
      //   store i32 %val.0.0, i32* %all0c.0.0
      //   %alloc.1.0 = getelementptr inbounds [2 x i32], [2 x i32]* %alloc.1,
      //   i32 0, i32 0
      //   %val.0.1 = extractelement <2 x i32> %243, i64 1
      //   store i32 %val.0.1, i32* %alloc.1.0
      //   %val.1 = extractvalue [2 x <2 x i32>] %val, 1
      //   %alloc.0.0 = getelementptr inbounds [2 x i32], [2 x i32]* %alloc.0,
      //   i32 0, i32 1
      //   %val.1.0 = extractelement <2 x i32> %248, i64 0
      //   store i32 %val.1.0, i32* %alloc.0.0
      //   %all0c.1.1 = getelementptr inbounds [2 x i32], [2 x i32]* %alloc.1,
      //   i32 0, i32 1
      //   %val.1.1 = extractelement <2 x i32> %248, i64 1
      //   store i32 %val.1.1, i32* %all0c.1.1
      ArrayType *AT = cast<ArrayType>(SIType);
      Type *i32Ty = Type::getInt32Ty(SIType->getContext());
      Value *zero = ConstantInt::get(i32Ty, 0);
      SmallVector<Value *, 8> idxList;
      idxList.emplace_back(zero);
      StoreVectorArray(AT, Val, NewElts, idxList, Builder);
      DeadInsts.push_back(SI);
    } else {
      // Replace:
      //   store { i32, i32 } %val, { i32, i32 }* %alloc
      // with:
      //   %val.0 = extractvalue { i32, i32 } %val, 0
      //   store i32 %val.0, i32* %alloc.0
      //   %val.1 = extractvalue { i32, i32 } %val, 1
      //   store i32 %val.1, i32* %alloc.1
      // (Also works for arrays instead of structs)
      Module *M = SI->getModule();
      for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
        Value *Extract = Builder.CreateExtractValue(Val, i, Val->getName());
        if (!HLMatrixLower::IsMatrixType(Extract->getType()))
          Builder.CreateStore(Extract, NewElts[i]);
        else {
          // Generate Matrix Store.
          HLModule::EmitHLOperationCall(
              Builder, HLOpcodeGroup::HLMatLoadStore,
              static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatStore),
              Extract->getType(), {NewElts[i], Extract}, *M);
        }
      }
      DeadInsts.push_back(SI);
    }
  } else {
    llvm_unreachable("other type don't need rewrite");
  }
}
/// RewriteMemIntrin - MI is a memcpy/memset/memmove from or to AI.
/// Rewrite it to copy or set the elements of the scalarized memory.
void SROA_Helper::RewriteMemIntrin(MemIntrinsic *MI, Instruction *Inst) {
  // If this is a memcpy/memmove, construct the other pointer as the
  // appropriate type.  The "Other" pointer is the pointer that goes to memory
  // that doesn't have anything to do with the alloca that we are promoting. For
  // memset, this Value* stays null.
  Value *OtherPtr = nullptr;
  unsigned MemAlignment = MI->getAlignment();
  if (MemTransferInst *MTI = dyn_cast<MemTransferInst>(MI)) { // memmove/memcopy
    if (Inst == MTI->getRawDest())
      OtherPtr = MTI->getRawSource();
    else {
      assert(Inst == MTI->getRawSource());
      OtherPtr = MTI->getRawDest();
    }
  }

  // If there is an other pointer, we want to convert it to the same pointer
  // type as AI has, so we can GEP through it safely.
  if (OtherPtr) {
    unsigned AddrSpace =
        cast<PointerType>(OtherPtr->getType())->getAddressSpace();

    // Remove bitcasts and all-zero GEPs from OtherPtr.  This is an
    // optimization, but it's also required to detect the corner case where
    // both pointer operands are referencing the same memory, and where
    // OtherPtr may be a bitcast or GEP that currently being rewritten.  (This
    // function is only called for mem intrinsics that access the whole
    // aggregate, so non-zero GEPs are not an issue here.)
    OtherPtr = OtherPtr->stripPointerCasts();

    // Copying the alloca to itself is a no-op: just delete it.
    if (OtherPtr == OldVal || OtherPtr == NewElts[0]) {
      // This code will run twice for a no-op memcpy -- once for each operand.
      // Put only one reference to MI on the DeadInsts list.
      for (SmallVectorImpl<Value *>::const_iterator I = DeadInsts.begin(),
                                                    E = DeadInsts.end();
           I != E; ++I)
        if (*I == MI)
          return;
      DeadInsts.push_back(MI);
      return;
    }

    // If the pointer is not the right type, insert a bitcast to the right
    // type.
    Type *NewTy =
        PointerType::get(OldVal->getType()->getPointerElementType(), AddrSpace);

    if (OtherPtr->getType() != NewTy)
      OtherPtr = new BitCastInst(OtherPtr, NewTy, OtherPtr->getName(), MI);
  }

  // Process each element of the aggregate.
  bool SROADest = MI->getRawDest() == Inst;

  Constant *Zero = Constant::getNullValue(Type::getInt32Ty(MI->getContext()));
  const DataLayout &DL = MI->getModule()->getDataLayout();

  for (unsigned i = 0, e = NewElts.size(); i != e; ++i) {
    // If this is a memcpy/memmove, emit a GEP of the other element address.
    Value *OtherElt = nullptr;
    unsigned OtherEltAlign = MemAlignment;

    if (OtherPtr) {
      Value *Idx[2] = {Zero,
                       ConstantInt::get(Type::getInt32Ty(MI->getContext()), i)};
      OtherElt = GetElementPtrInst::CreateInBounds(
          OtherPtr, Idx, OtherPtr->getName() + "." + Twine(i), MI);
      uint64_t EltOffset;
      PointerType *OtherPtrTy = cast<PointerType>(OtherPtr->getType());
      Type *OtherTy = OtherPtrTy->getElementType();
      if (StructType *ST = dyn_cast<StructType>(OtherTy)) {
        EltOffset = DL.getStructLayout(ST)->getElementOffset(i);
      } else {
        Type *EltTy = cast<SequentialType>(OtherTy)->getElementType();
        EltOffset = DL.getTypeAllocSize(EltTy) * i;
      }

      // The alignment of the other pointer is the guaranteed alignment of the
      // element, which is affected by both the known alignment of the whole
      // mem intrinsic and the alignment of the element.  If the alignment of
      // the memcpy (f.e.) is 32 but the element is at a 4-byte offset, then the
      // known alignment is just 4 bytes.
      OtherEltAlign = (unsigned)MinAlign(OtherEltAlign, EltOffset);
    }

    Value *EltPtr = NewElts[i];
    Type *EltTy = cast<PointerType>(EltPtr->getType())->getElementType();

    // If we got down to a scalar, insert a load or store as appropriate.
    if (EltTy->isSingleValueType()) {
      if (isa<MemTransferInst>(MI)) {
        if (SROADest) {
          // From Other to Alloca.
          Value *Elt = new LoadInst(OtherElt, "tmp", false, OtherEltAlign, MI);
          new StoreInst(Elt, EltPtr, MI);
        } else {
          // From Alloca to Other.
          Value *Elt = new LoadInst(EltPtr, "tmp", MI);
          new StoreInst(Elt, OtherElt, false, OtherEltAlign, MI);
        }
        continue;
      }
      assert(isa<MemSetInst>(MI));

      // If the stored element is zero (common case), just store a null
      // constant.
      Constant *StoreVal;
      if (ConstantInt *CI = dyn_cast<ConstantInt>(MI->getArgOperand(1))) {
        if (CI->isZero()) {
          StoreVal = Constant::getNullValue(EltTy); // 0.0, null, 0, <0,0>
        } else {
          // If EltTy is a vector type, get the element type.
          Type *ValTy = EltTy->getScalarType();

          // Construct an integer with the right value.
          unsigned EltSize = DL.getTypeSizeInBits(ValTy);
          APInt OneVal(EltSize, CI->getZExtValue());
          APInt TotalVal(OneVal);
          // Set each byte.
          for (unsigned i = 0; 8 * i < EltSize; ++i) {
            TotalVal = TotalVal.shl(8);
            TotalVal |= OneVal;
          }

          // Convert the integer value to the appropriate type.
          StoreVal = ConstantInt::get(CI->getContext(), TotalVal);
          if (ValTy->isPointerTy())
            StoreVal = ConstantExpr::getIntToPtr(StoreVal, ValTy);
          else if (ValTy->isFloatingPointTy())
            StoreVal = ConstantExpr::getBitCast(StoreVal, ValTy);
          assert(StoreVal->getType() == ValTy && "Type mismatch!");

          // If the requested value was a vector constant, create it.
          if (EltTy->isVectorTy()) {
            unsigned NumElts = cast<VectorType>(EltTy)->getNumElements();
            StoreVal = ConstantVector::getSplat(NumElts, StoreVal);
          }
        }
        new StoreInst(StoreVal, EltPtr, MI);
        continue;
      }
      // Otherwise, if we're storing a byte variable, use a memset call for
      // this element.
    }

    unsigned EltSize = DL.getTypeAllocSize(EltTy);
    if (!EltSize)
      continue;

    IRBuilder<> Builder(MI);

    // Finally, insert the meminst for this element.
    if (isa<MemSetInst>(MI)) {
      Builder.CreateMemSet(EltPtr, MI->getArgOperand(1), EltSize,
                           MI->isVolatile());
    } else {
      assert(isa<MemTransferInst>(MI));
      Value *Dst = SROADest ? EltPtr : OtherElt; // Dest ptr
      Value *Src = SROADest ? OtherElt : EltPtr; // Src ptr

      if (isa<MemCpyInst>(MI))
        Builder.CreateMemCpy(Dst, Src, EltSize, OtherEltAlign,
                             MI->isVolatile());
      else
        Builder.CreateMemMove(Dst, Src, EltSize, OtherEltAlign,
                              MI->isVolatile());
    }
  }
  DeadInsts.push_back(MI);
}

void SROA_Helper::RewriteBitCast(BitCastInst *BCI) {
  Type *DstTy = BCI->getType();
  Value *Val = BCI->getOperand(0);
  Type *SrcTy = Val->getType();
  if (!DstTy->isPointerTy()) {
    assert(0 && "Type mismatch.");
    return;
  }
  if (!SrcTy->isPointerTy()) {
    assert(0 && "Type mismatch.");
    return;
  }

  DstTy = DstTy->getPointerElementType();
  SrcTy = SrcTy->getPointerElementType();

  if (!DstTy->isStructTy()) {
    assert(0 && "Type mismatch.");
    return;
  }

  if (!SrcTy->isStructTy()) {
    assert(0 && "Type mismatch.");
    return;
  }
  // Only support bitcast to parent struct type.
  StructType *DstST = cast<StructType>(DstTy);
  StructType *SrcST = cast<StructType>(SrcTy);

  bool bTypeMatch = false;
  unsigned level = 0;
  while (SrcST) {
    level++;
    Type *EltTy = SrcST->getElementType(0);
    if (EltTy == DstST) {
      bTypeMatch = true;
      break;
    }
    SrcST = dyn_cast<StructType>(EltTy);
  }

  if (!bTypeMatch) {
    assert(0 && "Type mismatch.");
    return;
  }

  std::vector<Value*> idxList(level+1);
  ConstantInt *zeroIdx = ConstantInt::get(Type::getInt32Ty(Val->getContext()), 0);
  for (unsigned i=0;i<(level+1);i++)
    idxList[i] = zeroIdx;

  IRBuilder<> Builder(BCI);
  Instruction *GEP = cast<Instruction>(Builder.CreateInBoundsGEP(Val, idxList));
  BCI->replaceAllUsesWith(GEP);
  BCI->eraseFromParent();

  IRBuilder<> GEPBuilder(GEP);
  RewriteForGEP(cast<GEPOperator>(GEP), GEPBuilder);
}

/// RewriteCall - Replace OldVal with flattened NewElts in CallInst.
void SROA_Helper::RewriteCall(CallInst *CI) {
  HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
  Function *F = CI->getCalledFunction();
  if (group != HLOpcodeGroup::NotHL) {
    unsigned opcode = GetHLOpcode(CI);
    if (group == HLOpcodeGroup::HLIntrinsic) {
      IntrinsicOp IOP = static_cast<IntrinsicOp>(opcode);
      switch (IOP) {
      case IntrinsicOp::MOP_Append: {
        // Buffer Append already expand in code gen.
        // Must be OutputStream Append here.
        SmallVector<Value *, 4> flatArgs;
        for (Value *arg : CI->arg_operands()) {
          if (arg == OldVal) {
            // Flatten to arg.
            // Every Elt has a pointer type.
            // For Append, it's not a problem.
            for (Value *Elt : NewElts)
              flatArgs.emplace_back(Elt);
          } else
            flatArgs.emplace_back(arg);
        }

        SmallVector<Type *, 4> flatParamTys;
        for (Value *arg : flatArgs)
          flatParamTys.emplace_back(arg->getType());
        // Don't need flat return type for Append.
        FunctionType *flatFuncTy =
            FunctionType::get(CI->getType(), flatParamTys, false);
        Function *flatF =
            GetOrCreateHLFunction(*F->getParent(), flatFuncTy, group, opcode);
        IRBuilder<> Builder(CI);
        // Append return void, don't need to replace CI with flatCI.
        Builder.CreateCall(flatF, flatArgs);

        DeadInsts.push_back(CI);
      } break;
      default:
        DXASSERT(0, "cannot flatten hlsl intrinsic.");
      }
    }
    // TODO: check other high level dx operations if need to.
  } else {
    DXASSERT(0, "should done at inline");
  }
}

/// RewriteForConstExpr - Rewrite the GEP which is ConstantExpr.
void SROA_Helper::RewriteForConstExpr(ConstantExpr *CE, IRBuilder<> &Builder) {
  if (GEPOperator *GEP = dyn_cast<GEPOperator>(CE)) {
    if (OldVal == GEP->getPointerOperand()) {
      // Flatten GEP.
      RewriteForGEP(GEP, Builder);
      return;
    }
  }
  // Skip unused CE. 
  if (CE->use_empty())
    return;

  Instruction *constInst = CE->getAsInstruction();
  Builder.Insert(constInst);
  // Replace CE with constInst.
  for (Value::use_iterator UI = CE->use_begin(), E = CE->use_end(); UI != E;) {
    Use &TheUse = *UI++;
    if (isa<Instruction>(TheUse.getUser()))
      TheUse.set(constInst);
    else {
      RewriteForConstExpr(cast<ConstantExpr>(TheUse.getUser()), Builder);
    }
  }
}
/// RewriteForScalarRepl - OldVal is being split into NewElts, so rewrite
/// users of V, which references it, to use the separate elements.
void SROA_Helper::RewriteForScalarRepl(Value *V, IRBuilder<> &Builder) {

  for (Value::use_iterator UI = V->use_begin(), E = V->use_end(); UI != E;) {
    Use &TheUse = *UI++;

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(TheUse.getUser())) {
      RewriteForConstExpr(CE, Builder);
      continue;
    }
    Instruction *User = cast<Instruction>(TheUse.getUser());

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      IRBuilder<> Builder(GEP);
      RewriteForGEP(cast<GEPOperator>(GEP), Builder);
    } else if (LoadInst *ldInst = dyn_cast<LoadInst>(User))
      RewriteForLoad(ldInst);
    else if (StoreInst *stInst = dyn_cast<StoreInst>(User))
      RewriteForStore(stInst);
    else if (MemIntrinsic *MI = dyn_cast<MemIntrinsic>(User))
      RewriteMemIntrin(MI, cast<Instruction>(V));
    else if (CallInst *CI = dyn_cast<CallInst>(User)) 
      RewriteCall(CI);
    else if (BitCastInst *BCI = dyn_cast<BitCastInst>(User))
      RewriteBitCast(BCI);
    else {
      assert(0 && "not support.");
    }
  }
}

static ArrayType *CreateNestArrayTy(Type *FinalEltTy,
                                    ArrayRef<ArrayType *> nestArrayTys) {
  Type *newAT = FinalEltTy;
  for (auto ArrayTy = nestArrayTys.rbegin(), E=nestArrayTys.rend(); ArrayTy != E;
       ++ArrayTy)
    newAT = ArrayType::get(newAT, (*ArrayTy)->getNumElements());
  return cast<ArrayType>(newAT);
}

/// DoScalarReplacement - Split V into AllocaInsts with Builder and save the new AllocaInsts into Elts.
/// Then do SROA on V.
bool SROA_Helper::DoScalarReplacement(Value *V, std::vector<Value *> &Elts,
                                      IRBuilder<> &Builder, bool bFlatVector,
                                      bool hasPrecise, DxilTypeSystem &typeSys,
                                      SmallVector<Value *, 32> &DeadInsts) {
  DEBUG(dbgs() << "Found inst to SROA: " << *V << '\n');
  Type *Ty = V->getType();
  // Skip none pointer types.
  if (!Ty->isPointerTy())
    return false;

  Ty = Ty->getPointerElementType();
  // Skip none aggregate types.
  if (!Ty->isAggregateType())
    return false;
  // Skip matrix types.
  if (HLMatrixLower::IsMatrixType(Ty))
    return false;
  
  if (StructType *ST = dyn_cast<StructType>(Ty)) {
    // Skip HLSL object types.
    if (HLModule::IsHLSLObjectType(ST)) {
      return false;
    }

    unsigned numTypes = ST->getNumContainedTypes();
    Elts.reserve(numTypes);
    DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ST);
    // Skip empty struct.
    if (SA && SA->IsEmptyStruct())
      return true;
    for (int i = 0, e = numTypes; i != e; ++i) {
      AllocaInst *NA = Builder.CreateAlloca(ST->getContainedType(i), nullptr, V->getName() + "." + Twine(i));
      bool markPrecise = hasPrecise;
      if (SA) {
        DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
        markPrecise |= FA.IsPrecise();
      }
      if (markPrecise)
        HLModule::MarkPreciseAttributeWithMetadata(NA);
      Elts.push_back(NA);
    }
  } else {
    ArrayType *AT = cast<ArrayType>(Ty);
    if (AT->getNumContainedTypes() == 0) {
      // Skip case like [0 x %struct].
      return false;
    }
    Type *ElTy = AT->getElementType();
    SmallVector<ArrayType *, 4> nestArrayTys;

    nestArrayTys.emplace_back(AT);
    // support multi level of array
    while (ElTy->isArrayTy()) {
      ArrayType *ElAT = cast<ArrayType>(ElTy);
      nestArrayTys.emplace_back(ElAT);
      ElTy = ElAT->getElementType();
    }

    if (ElTy->isStructTy() &&
        // Skip Matrix type.
        !HLMatrixLower::IsMatrixType(ElTy)) {
      // Skip HLSL object types.
      if (HLModule::IsHLSLObjectType(ElTy)) {
        return false;
      }

      // for array of struct
      // split into arrays of struct elements
      StructType *ElST = cast<StructType>(ElTy);
      unsigned numTypes = ElST->getNumContainedTypes();
      Elts.reserve(numTypes);
      DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ElST);
      // Skip empty struct.
      if (SA && SA->IsEmptyStruct())
        return true;
      for (int i = 0, e = numTypes; i != e; ++i) {
        AllocaInst *NA = Builder.CreateAlloca(
            CreateNestArrayTy(ElST->getContainedType(i), nestArrayTys), nullptr,
            V->getName() + "." + Twine(i));
        bool markPrecise = hasPrecise;
        if (SA) {
          DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
          markPrecise |= FA.IsPrecise();
        }
        if (markPrecise)
          HLModule::MarkPreciseAttributeWithMetadata(NA);
        Elts.push_back(NA);
      }
    } else if (ElTy->isVectorTy()) {
      // Skip vector if required.
      if (!bFlatVector)
        return false;

      // for array of vector
      // split into arrays of scalar
      VectorType *ElVT = cast<VectorType>(ElTy);
      Elts.reserve(ElVT->getNumElements());

      ArrayType *scalarArrayTy = CreateNestArrayTy(ElVT->getElementType(), nestArrayTys);

      for (int i = 0, e = ElVT->getNumElements(); i != e; ++i) {
        AllocaInst *NA = Builder.CreateAlloca(scalarArrayTy, nullptr, 
                           V->getName() + "." + Twine(i));
        if (hasPrecise)
          HLModule::MarkPreciseAttributeWithMetadata(NA);
        Elts.push_back(NA);
      }
    } else
      // Skip array of basic types.
      return false;
  }
  
  // Now that we have created the new alloca instructions, rewrite all the
  // uses of the old alloca.
  SROA_Helper helper(V, Elts, DeadInsts);
  helper.RewriteForScalarRepl(V, Builder);

  return true;
}

/// DoScalarReplacement - Split V into AllocaInsts with Builder and save the new AllocaInsts into Elts.
/// Then do SROA on V.
bool SROA_Helper::DoScalarReplacement(GlobalVariable *GV, std::vector<Value *> &Elts,
                                      IRBuilder<> &Builder, bool bFlatVector,
                                      bool hasPrecise, DxilTypeSystem &typeSys,
                                      SmallVector<Value *, 32> &DeadInsts) {
  DEBUG(dbgs() << "Found inst to SROA: " << *GV << '\n');
  Type *Ty = GV->getType();
  // Skip none pointer types.
  if (!Ty->isPointerTy())
    return false;

  Ty = Ty->getPointerElementType();
  // Skip none aggregate types.
  if (!Ty->isAggregateType() && !bFlatVector)
    return false;
  // Skip basic types.
  if (Ty->isSingleValueType() && !Ty->isVectorTy())
    return false;
  // Skip matrix types.
  if (HLMatrixLower::IsMatrixType(Ty))
    return false;

  Module *M = GV->getParent();
  Constant *Init = GV->getInitializer();
  if (!Init)
    Init = UndefValue::get(Ty);
  bool isConst = GV->isConstant();

  GlobalVariable::ThreadLocalMode TLMode = GV->getThreadLocalMode();
  unsigned AddressSpace = GV->getType()->getAddressSpace();
  GlobalValue::LinkageTypes linkage = GV->getLinkage();

  if (StructType *ST = dyn_cast<StructType>(Ty)) {
    // Skip HLSL object types.
    if (HLModule::IsHLSLObjectType(ST))
      return false;
    unsigned numTypes = ST->getNumContainedTypes();
    Elts.reserve(numTypes);
    //DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ST);
    for (int i = 0, e = numTypes; i != e; ++i) {
      Constant *EltInit = cast<Constant>(Builder.CreateExtractValue(Init, i));
      GlobalVariable *EltGV = new llvm::GlobalVariable(
          *M, ST->getContainedType(i), /*IsConstant*/ isConst, linkage,
          /*InitVal*/ EltInit, GV->getName() + "." + Twine(i),
          /*InsertBefore*/ nullptr, TLMode, AddressSpace);

      //DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
      // TODO: set precise.
      // if (hasPrecise || FA.IsPrecise())
      //  HLModule::MarkPreciseAttributeWithMetadata(NA);
      Elts.push_back(EltGV);
    }
  } else if (VectorType *VT = dyn_cast<VectorType>(Ty)) {
    // TODO: support dynamic indexing on vector by change it to array.
    unsigned numElts = VT->getNumElements();
    Elts.reserve(numElts);
    Type *EltTy = VT->getElementType();
    //DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ST);
    for (int i = 0, e = numElts; i != e; ++i) {
      Constant *EltInit = cast<Constant>(Builder.CreateExtractElement(Init, i));
      GlobalVariable *EltGV = new llvm::GlobalVariable(
          *M, EltTy, /*IsConstant*/ isConst, linkage,
          /*InitVal*/ EltInit, GV->getName() + "." + Twine(i),
          /*InsertBefore*/ nullptr, TLMode, AddressSpace);

      //DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
      // TODO: set precise.
      // if (hasPrecise || FA.IsPrecise())
      //  HLModule::MarkPreciseAttributeWithMetadata(NA);
      Elts.push_back(EltGV);
    }
  } else {
    ArrayType *AT = cast<ArrayType>(Ty);
    if (AT->getNumContainedTypes() == 0) {
      // Skip case like [0 x %struct].
      return false;
    }
    Type *ElTy = AT->getElementType();
    SmallVector<ArrayType *, 4> nestArrayTys;

    nestArrayTys.emplace_back(AT);
    // support multi level of array
    while (ElTy->isArrayTy()) {
      ArrayType *ElAT = cast<ArrayType>(ElTy);
      nestArrayTys.emplace_back(ElAT);
      ElTy = ElAT->getElementType();
    }

    if (ElTy->isStructTy() &&
        // Skip Matrix type.
        !HLMatrixLower::IsMatrixType(ElTy)) {
      // for array of struct
      // split into arrays of struct elements
      StructType *ElST = cast<StructType>(ElTy);
      unsigned numTypes = ElST->getNumContainedTypes();
      Elts.reserve(numTypes);
      //DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ElST);
      for (int i = 0, e = numTypes; i != e; ++i) {
        Type *EltTy =
            CreateNestArrayTy(ElST->getContainedType(i), nestArrayTys);
        // Don't need InitVal, struct type will use store to init.
        Constant *EltInit = UndefValue::get(EltTy);
        GlobalVariable *EltGV = new llvm::GlobalVariable(
            *M, EltTy, /*IsConstant*/ isConst, linkage,
            /*InitVal*/ EltInit, GV->getName() + "." + Twine(i),
            /*InsertBefore*/ nullptr, TLMode, AddressSpace);

        //DxilFieldAnnotation &FA = SA->GetFieldAnnotation(i);
        // TODO: set precise.
        // if (hasPrecise || FA.IsPrecise())
        //  HLModule::MarkPreciseAttributeWithMetadata(NA);
        Elts.push_back(EltGV);
      }
    } else if (ElTy->isVectorTy()) {
      // Skip vector if required.
      if (!bFlatVector)
        return false;

      // for array of vector
      // split into arrays of scalar
      VectorType *ElVT = cast<VectorType>(ElTy);
      Elts.reserve(ElVT->getNumElements());

      ArrayType *scalarArrayTy =
          CreateNestArrayTy(ElVT->getElementType(), nestArrayTys);

      for (int i = 0, e = ElVT->getNumElements(); i != e; ++i) {
        // Don't need InitVal, struct type will use store to init.
        Constant *EltInit = UndefValue::get(scalarArrayTy);
        GlobalVariable *EltGV = new llvm::GlobalVariable(
            *M, scalarArrayTy, /*IsConstant*/ isConst, linkage,
            /*InitVal*/ EltInit, GV->getName() + "." + Twine(i),
            /*InsertBefore*/ nullptr, TLMode, AddressSpace);
        // TODO: set precise.
        // if (hasPrecise)
        //  HLModule::MarkPreciseAttributeWithMetadata(NA);
        Elts.push_back(EltGV);
      }
    } else
      // Skip array of basic types.
      return false;
  }

  // Now that we have created the new alloca instructions, rewrite all the
  // uses of the old alloca.
  SROA_Helper helper(GV, Elts, DeadInsts);
  helper.RewriteForScalarRepl(GV, Builder);

  return true;
}
/// MarkEmptyStructUsers - Add instruction related to Empty struct to DeadInsts.
void SROA_Helper::MarkEmptyStructUsers(Value *V, SmallVector<Value *, 32> &DeadInsts) {
  for (User *U : V->users()) {
    MarkEmptyStructUsers(U, DeadInsts);
  }
  if (Instruction *I = dyn_cast<Instruction>(V)) {
    // Only need to add no use inst here.
    // DeleteDeadInst will delete everything.
    if (I->user_empty())
      DeadInsts.emplace_back(I);
  }
}

bool SROA_Helper::IsEmptyStructType(Type *Ty, DxilTypeSystem &typeSys) {
  if (isa<ArrayType>(Ty))
    Ty = Ty->getArrayElementType();

  if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (!HLMatrixLower::IsMatrixType(Ty)) {
      DxilStructAnnotation *SA = typeSys.GetStructAnnotation(ST);
      if (SA && SA->IsEmptyStruct())
        return true;
    }
  }
  return false;
}

//===----------------------------------------------------------------------===//
// SROA on function parameters.
//===----------------------------------------------------------------------===//

namespace {
class SROA_Parameter_HLSL : public ModulePass {
  HLModule *m_pHLModule;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit SROA_Parameter_HLSL() : ModulePass(ID) {}
  const char *getPassName() const override { return "SROA Parameter HLSL"; }

  bool runOnModule(Module &M) override { 
    m_pHLModule = &M.GetOrCreateHLModule();

    // Load up debug information, to cross-reference values and the instructions
    // used to load them.
    m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

    std::deque<Function *> WorkList;
    for (Function &F : M.functions()) {
      HLOpcodeGroup group = GetHLOpcodeGroup(&F);
      // Skip HL operations.
      if (group != HLOpcodeGroup::NotHL || group == HLOpcodeGroup::HLExtIntrinsic)
        continue;

      if (F.isDeclaration()) {
        // Skip llvm intrinsic.
        if (F.isIntrinsic())
          continue;
        // Skip unused external function.
        if (F.user_empty())
          continue;
      }

      WorkList.emplace_back(&F);
    }

    // Process the worklist
    while (!WorkList.empty()) {
      Function *F = WorkList.front();
      WorkList.pop_front();
      createFlattenedFunction(F);
    }

    // Replace functions with flattened version when we flat all the functions.
    for (auto Iter : funcMap)
      replaceCall(Iter.first, Iter.second);

    // Remove flattened functions.
    for (auto Iter : funcMap) {
      Function *F = Iter.first;
      F->eraseFromParent();
    }

    // Flatten internal global.
    std::vector<GlobalVariable *> staticGVs;
    for (GlobalVariable &GV : M.globals()) {
      if (HLModule::IsStaticGlobal(&GV) ||
          HLModule::IsSharedMemoryGlobal(&GV)) {
        staticGVs.emplace_back(&GV);
      } else {
        // merge GEP use for global.
        MergeGepUse(&GV);
      }
    }

    for (GlobalVariable *GV : staticGVs)
      flattenGlobal(GV);

    // Remove unused internal global.
    staticGVs.clear();
    for (GlobalVariable &GV : M.globals()) {
      if (HLModule::IsStaticGlobal(&GV) ||
          HLModule::IsSharedMemoryGlobal(&GV)) {
        staticGVs.emplace_back(&GV);
      }
    }

    for (GlobalVariable *GV : staticGVs) {
      bool onlyStoreUse = true;
      for (User *user : GV->users()) {
        if (isa<StoreInst>(user))
          continue;
        if (isa<ConstantExpr>(user) && user->user_empty())
          continue;

        // Check matrix store.
        if (HLMatrixLower::IsMatrixType(
                GV->getType()->getPointerElementType())) {
          if (CallInst *CI = dyn_cast<CallInst>(user)) {
            if (GetHLOpcodeGroupByName(CI->getCalledFunction()) ==
                HLOpcodeGroup::HLMatLoadStore) {
              HLMatLoadStoreOpcode opcode =
                  static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(CI));
              if (opcode == HLMatLoadStoreOpcode::ColMatStore ||
                  opcode == HLMatLoadStoreOpcode::RowMatStore)
                continue;
            }
          }
        }

        onlyStoreUse = false;
        break;
      }
      if (onlyStoreUse) {
        for (auto UserIt = GV->user_begin(); UserIt != GV->user_end();) {
          Value *User = *(UserIt++);
          if (Instruction *I = dyn_cast<Instruction>(User)) {
            I->eraseFromParent();
          }
          else {
            ConstantExpr *CE = cast<ConstantExpr>(User);
            CE->dropAllReferences();
          }
        }
        GV->eraseFromParent();
      }
    }

    return true;
  }

private:
  void DeleteDeadInstructions();
  void moveFunctionBody(Function *F, Function *flatF);
  void replaceCall(Function *F, Function *flatF);
  void createFlattenedFunction(Function *F);
  void createFlattenedFunctionCall(Function *F, Function *flatF, CallInst *CI);
  void flattenArgument(Function *F, Value *Arg,
                  DxilParameterAnnotation &paramAnnotation,
                  std::vector<Value *> &FlatParamList,
                  std::vector<DxilParameterAnnotation> &FlatRetAnnotationList,
                  IRBuilder<> &Builder, DbgDeclareInst *DDI);
  void allocateSemanticIndex(
    std::vector<DxilParameterAnnotation> &FlatAnnotationList,
    unsigned startArgIndex, llvm::StringMap<Type *> &semanticTypeMap);
  bool hasDynamicVectorIndexing(Value *V);
  void flattenGlobal(GlobalVariable *GV);
  /// DeadInsts - Keep track of instructions we have made dead, so that
  /// we can remove them after we are done working.
  SmallVector<Value *, 32> DeadInsts;
  // Map from orginal function to the flatten version.
  std::unordered_map<Function *, Function *> funcMap;
  bool m_HasDbgInfo;
};
}

char SROA_Parameter_HLSL::ID = 0;

INITIALIZE_PASS(SROA_Parameter_HLSL, "scalarrepl-param-hlsl",
  "Scalar Replacement of Aggregates HLSL (parameters)", false,
  false)

/// DeleteDeadInstructions - Erase instructions on the DeadInstrs list,
/// recursively including all their operands that become trivially dead.
void SROA_Parameter_HLSL::DeleteDeadInstructions() {
  while (!DeadInsts.empty()) {
    Instruction *I = cast<Instruction>(DeadInsts.pop_back_val());

    for (User::op_iterator OI = I->op_begin(), E = I->op_end(); OI != E; ++OI)
      if (Instruction *U = dyn_cast<Instruction>(*OI)) {
        // Zero out the operand and see if it becomes trivially dead.
        // (But, don't add allocas to the dead instruction list -- they are
        // already on the worklist and will be deleted separately.)
        *OI = nullptr;
        if (isInstructionTriviallyDead(U) && !isa<AllocaInst>(U))
          DeadInsts.push_back(U);
      }

    I->eraseFromParent();
  }
}

bool SROA_Parameter_HLSL::hasDynamicVectorIndexing(Value *V) {
  for (User *U : V->users()) {
    if (!U->getType()->isPointerTy())
      continue;

    if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {

      gep_type_iterator GEPIt = gep_type_begin(U), E = gep_type_end(U);

      for (; GEPIt != E; ++GEPIt) {
        if (isa<VectorType>(*GEPIt)) {
          Value *VecIdx = GEPIt.getOperand();
          if (!isa<ConstantInt>(VecIdx))
            return true;
        }
      }
    }
  }
  return false;
}

void SROA_Parameter_HLSL::flattenGlobal(GlobalVariable *GV) {

  Type *Ty = GV->getType()->getPointerElementType();
  // Skip basic types.
  if (!Ty->isAggregateType() && !Ty->isVectorTy())
    return;

  std::deque<Value *> WorkList;
  WorkList.push_back(GV);
  // merge GEP use for global.
  MergeGepUse(GV);
  Function *Entry = m_pHLModule->GetEntryFunction();
  
  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();

  IRBuilder<> Builder(Entry->getEntryBlock().getFirstInsertionPt());
  std::vector<Instruction*> deadAllocas;

  const DataLayout &DL = GV->getParent()->getDataLayout();
  unsigned debugOffset = 0;
  std::unordered_map<Value*, StringRef> EltNameMap;
  bool isFlattened = false;
  // Process the worklist
  while (!WorkList.empty()) {
    GlobalVariable *EltGV = cast<GlobalVariable>(WorkList.front());
    WorkList.pop_front();
    // Flat Global vector if no dynamic vector indexing.
    bool bFlatVector = !hasDynamicVectorIndexing(EltGV);

    std::vector<Value *> Elts;
    bool SROAed = SROA_Helper::DoScalarReplacement(
        EltGV, Elts, Builder, bFlatVector,
        // TODO: set precise.
        /*hasPrecise*/ false,
        dxilTypeSys, DeadInsts);

    if (SROAed) {
      // Push Elts into workList.
      // Use rbegin to make sure the order not change.
      for (auto iter = Elts.rbegin(); iter != Elts.rend(); iter++) {
        WorkList.push_front(*iter);
        if (m_HasDbgInfo) {
          StringRef EltName = (*iter)->getName().ltrim(GV->getName());
          EltNameMap[*iter] = EltName;
        }
      }
      EltGV->removeDeadConstantUsers();
      // Now erase any instructions that were made dead while rewriting the
      // alloca.
      DeleteDeadInstructions();
      ++NumReplaced;
    } else {
      // Add debug info for flattened globals.
      if (m_HasDbgInfo && GV != EltGV) {
        DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
        Type *Ty = EltGV->getType()->getElementType();
        unsigned size = DL.getTypeAllocSizeInBits(Ty);
        unsigned align = DL.getPrefTypeAlignment(Ty);
        HLModule::CreateElementGlobalVariableDebugInfo(
            GV, Finder, EltGV, size, align, debugOffset,
            EltNameMap[EltGV]);
        debugOffset += size;
      }
      if (GV != EltGV)
        isFlattened = true;
    }
  }
  if (isFlattened) {
    GV->removeDeadConstantUsers();
    GV->eraseFromParent();
  }
}

static DxilFieldAnnotation &GetEltAnnotation(Type *Ty, unsigned idx, DxilFieldAnnotation &annotation, DxilTypeSystem &dxilTypeSys) {
  while (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();
  if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (HLMatrixLower::IsMatrixType(Ty))
      return annotation;
    DxilStructAnnotation *SA = dxilTypeSys.GetStructAnnotation(ST);
    if (SA) {
      DxilFieldAnnotation &FA = SA->GetFieldAnnotation(idx);
      return FA;
    }
  }
  return annotation;  
}

static Type *GetArrayEltTy(Type *Ty) {
  if (isa<PointerType>(Ty))
    Ty = Ty->getPointerElementType();
  while (isa<ArrayType>(Ty)) {
    Ty = Ty->getArrayElementType();
  }
  return Ty;
}

// Note: Semantic index allocation.
// Semantic index is allocated base on linear layout.
// For following code
/*
    struct S {
     float4 m;
     float4 m2;
    };
    S  s[2] : semantic;

    struct S2 {
     float4 m[2];
     float4 m2[2];
    };

    S2  s2 : semantic;
*/
//  The semantic index is like this:
//  s[0].m  : semantic0
//  s[0].m2 : semantic1
//  s[1].m  : semantic2
//  s[1].m2 : semantic3

//  s2.m[0]  : semantic0
//  s2.m[1]  : semantic1
//  s2.m2[0] : semantic2
//  s2.m2[1] : semantic3

//  But when flatten argument, the result is like this:
//  float4 s_m[2], float4 s_m2[2].
//  float4 s2_m[2], float4 s2_m2[2].

// To do the allocation, need to map from each element to its flattened argument.
// Say arg index of float4 s_m[2] is 0, float4 s_m2[2] is 1.
// Need to get 0 from s[0].m and s[1].m, get 1 from s[0].m2 and s[1].m2.


// Allocate the argments with same semantic string from type where the
// semantic starts( S2 for s2.m[2] and s2.m2[2]).
// Iterate each elements of the type, save the semantic index and update it.
// The map from element to the arg ( s[0].m2 -> s.m2[2]) is done by argIdx.
// ArgIdx only inc by 1 when finish a struct field.
static unsigned AllocateSemanticIndex(
    Type *Ty, unsigned &semIndex, unsigned argIdx, unsigned endArgIdx,
    std::vector<DxilParameterAnnotation> &FlatAnnotationList) {
  if (Ty->isPointerTy()) {
    return AllocateSemanticIndex(Ty->getPointerElementType(), semIndex, argIdx,
                                 endArgIdx, FlatAnnotationList);
  } else if (Ty->isArrayTy()) {
    unsigned arraySize = Ty->getArrayNumElements();
    unsigned updatedArgIdx = argIdx;
    Type *EltTy = Ty->getArrayElementType();
    for (unsigned i = 0; i < arraySize; i++) {
      updatedArgIdx = AllocateSemanticIndex(EltTy, semIndex, argIdx, endArgIdx,
                                            FlatAnnotationList);
    }
    return updatedArgIdx;
  } else if (Ty->isStructTy() && !HLMatrixLower::IsMatrixType(Ty)) {
    unsigned fieldsCount = Ty->getStructNumElements();
    for (unsigned i = 0; i < fieldsCount; i++) {
      Type *EltTy = Ty->getStructElementType(i);
      argIdx = AllocateSemanticIndex(EltTy, semIndex, argIdx, endArgIdx,
                                     FlatAnnotationList);
      // Update argIdx by 1.
      argIdx++;
    }
    return argIdx;
  } else {
    DXASSERT(argIdx < endArgIdx, "arg index out of bound");
    DxilParameterAnnotation &paramAnnotation = FlatAnnotationList[argIdx];
    // Save semIndex.
    paramAnnotation.AppendSemanticIndex(semIndex);
    // Get element size.
    unsigned rows = 1;
    if (paramAnnotation.HasMatrixAnnotation()) {
      const DxilMatrixAnnotation &matrix =
          paramAnnotation.GetMatrixAnnotation();
      if (matrix.Orientation == MatrixOrientation::RowMajor) {
        rows = matrix.Rows;
      } else {
        DXASSERT(matrix.Orientation == MatrixOrientation::ColumnMajor, "");
        rows = matrix.Cols;
      }
    }
    // Update semIndex.
    semIndex += rows;

    return argIdx;
  }
}

void SROA_Parameter_HLSL::allocateSemanticIndex(
    std::vector<DxilParameterAnnotation> &FlatAnnotationList,
    unsigned startArgIndex, llvm::StringMap<Type *> &semanticTypeMap) {
  unsigned endArgIndex = FlatAnnotationList.size();

  // Allocate semantic index.
  for (unsigned i = startArgIndex; i < endArgIndex; ++i) {
    // Group by semantic names.
    DxilParameterAnnotation &flatParamAnnotation = FlatAnnotationList[i];
    const std::string &semantic = flatParamAnnotation.GetSemanticString();

    // If semantic is undefined, an error will be emitted elsewhere.  For now,
    // we should avoid asserting.
    if (semantic.empty())
      continue;

    unsigned semGroupEnd = i + 1;
    while (semGroupEnd < endArgIndex &&
           FlatAnnotationList[semGroupEnd].GetSemanticString() == semantic) {
      ++semGroupEnd;
    }

    StringRef baseSemName; // The 'FOO' in 'FOO1'.
    uint32_t semIndex;     // The '1' in 'FOO1'
    // Split semName and index.
    Semantic::DecomposeNameAndIndex(semantic, &baseSemName, &semIndex);

    DXASSERT(semanticTypeMap.count(semantic) > 0, "Must has semantic type");
    Type *semanticTy = semanticTypeMap[semantic];

    AllocateSemanticIndex(semanticTy, semIndex, /*argIdx*/ i,
                          /*endArgIdx*/ semGroupEnd, FlatAnnotationList);
    // Update i.
    i = semGroupEnd - 1;
  }
}

void SROA_Parameter_HLSL::flattenArgument(
    Function *F, Value *Arg, DxilParameterAnnotation &paramAnnotation,
    std::vector<Value *> &FlatParamList,
    std::vector<DxilParameterAnnotation> &FlatAnnotationList, IRBuilder<> &Builder,
    DbgDeclareInst *DDI) {
  std::deque<Value *> WorkList;
  WorkList.push_back(Arg);

  Function *Entry = m_pHLModule->GetEntryFunction();
  bool hasShaderInputOutput = F == Entry;

  if (m_pHLModule->HasHLFunctionProps(Entry)) {
    HLFunctionProps &funcProps = m_pHLModule->GetHLFunctionProps(Entry);
    if (funcProps.shaderKind == DXIL::ShaderKind::Hull) {
      Function *patchConstantFunc = funcProps.ShaderProps.HS.patchConstantFunc;
      hasShaderInputOutput |= F == patchConstantFunc;
    }
  }
  Type *opcodeTy = Type::getInt32Ty(F->getContext());

  unsigned startArgIndex = FlatAnnotationList.size();

  // Map from value to annotation.
  std::unordered_map<Value *, DxilFieldAnnotation> annotationMap;
  annotationMap[Arg] = paramAnnotation;

  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();

  const std::string &semantic = paramAnnotation.GetSemanticString();
  bool bSemOverride = !semantic.empty();

  bool bForParam = F == Builder.GetInsertPoint()->getParent()->getParent();

  // Map from semantic string to type.
  llvm::StringMap<Type *> semanticTypeMap;
  // Original semantic type.
  if (!semantic.empty()) {
    semanticTypeMap[semantic] = Arg->getType();
  }

  std::vector<Instruction*> deadAllocas;

  DIBuilder DIB(*F->getParent(), /*AllowUnresolved*/ false);
  unsigned debugOffset = 0;
  const DataLayout &DL = F->getParent()->getDataLayout();

  // Process the worklist
  while (!WorkList.empty()) {
    Value *V = WorkList.front();
    WorkList.pop_front();

    // Do not skip unused parameter.

    DxilFieldAnnotation &annotation = annotationMap[V];

    std::vector<Value *> Elts;
    // Not flat vector for entry function currently.
    bool SROAed = SROA_Helper::DoScalarReplacement(
        V, Elts, Builder, /*bFlatVector*/ false, annotation.IsPrecise(),
        dxilTypeSys, DeadInsts);

    if (SROAed) {
      Type *Ty = V->getType()->getPointerElementType();
      // Skip empty struct parameters.
      if (SROA_Helper::IsEmptyStructType(Ty, dxilTypeSys)) {
        SROA_Helper::MarkEmptyStructUsers(V, DeadInsts);
        DeleteDeadInstructions();
        continue;
      }

      // Push Elts into workList.
      // Use rbegin to make sure the order not change.
      for (auto iter = Elts.rbegin(); iter != Elts.rend(); iter++)
        WorkList.push_front(*iter);

      bool precise = annotation.IsPrecise();
      const std::string &semantic = annotation.GetSemanticString();
      hlsl::InterpolationMode interpMode = annotation.GetInterpolationMode();
      
      for (unsigned i=0;i<Elts.size();i++) {
        Value *Elt = Elts[i];
        DxilFieldAnnotation EltAnnotation = GetEltAnnotation(Ty, i, annotation, dxilTypeSys);
        const std::string &eltSem = EltAnnotation.GetSemanticString();

        if (!semantic.empty()) {
          if (!eltSem.empty()) {
            // TODO: warning for override the semantic in EltAnnotation.
          }
          // Just save parent semantic here, allocate later.
          EltAnnotation.SetSemanticString(semantic);
        } else if (!eltSem.empty() &&
                 semanticTypeMap.count(eltSem) == 0) {
          Type *EltTy = GetArrayEltTy(Ty);
          DXASSERT(EltTy->isStructTy(), "must be a struct type to has semantic.");
          semanticTypeMap[eltSem] = EltTy->getStructElementType(i);
        }

        if (precise)
          EltAnnotation.SetPrecise();

        if (EltAnnotation.GetInterpolationMode().GetKind() == DXIL::InterpolationMode::Undefined)
          EltAnnotation.SetInterpolationMode(interpMode);

        annotationMap[Elt] = EltAnnotation;
      }

      annotationMap.erase(V);

      // Now erase any instructions that were made dead while rewriting the
      // alloca.
      DeleteDeadInstructions();

      DXASSERT_NOMSG(V->user_empty());
      ++NumReplaced;
      if (Instruction *I = dyn_cast<Instruction>(V))
        deadAllocas.emplace_back(I);
    } else {
      if (bSemOverride) {
        if (!annotation.GetSemanticString().empty()) {
          // TODO: warning for override the semantic in EltAnnotation.
        }
        // Just save parent semantic here, allocate later.
        annotation.SetSemanticString(semantic);
      }

      // Flatten array of SV_Target.
      StringRef semanticStr = annotation.GetSemanticString();
      if (semanticStr.upper().find("SV_TARGET") == 0 &&
          V->getType()->getPointerElementType()->isArrayTy()) {
        Type *Ty = cast<ArrayType>(V->getType()->getPointerElementType());
        StringRef targetStr;
        unsigned  targetIndex;
        Semantic::DecomposeNameAndIndex(semanticStr, &targetStr, &targetIndex);
        // Replace target parameter with local target.
        AllocaInst *localTarget = Builder.CreateAlloca(Ty);
        V->replaceAllUsesWith(localTarget);
        unsigned arraySize = 1;
        std::vector<unsigned> arraySizeList;
        while (Ty->isArrayTy()) {
          unsigned size = Ty->getArrayNumElements();
          arraySizeList.emplace_back(size);
          arraySize *= size;
          Ty = Ty->getArrayElementType();
        }

        unsigned arrayLevel = arraySizeList.size();
        std::vector<unsigned> arrayIdxList(arrayLevel, 0);

        // Create flattened target.
        DxilFieldAnnotation EltAnnotation = annotation;
        for (unsigned i=0;i<arraySize;i++) {
          Value *Elt = Builder.CreateAlloca(Ty);
          EltAnnotation.SetSemanticString(targetStr.str()+std::to_string(targetIndex+i));

          // Add semantic type.
          semanticTypeMap[EltAnnotation.GetSemanticString()] = Ty;

          annotationMap[Elt] = EltAnnotation;
          WorkList.push_front(Elt);
          // Copy local target to flattened target.
          std::vector<Value*> idxList(arrayLevel+1);
          idxList[0] = Builder.getInt32(0);
          for (unsigned idx=0;idx<arrayLevel; idx++) {
            idxList[idx+1] = Builder.getInt32(arrayIdxList[idx]);
          }

          if (bForParam) {
            // If Argument, copy before each return.
            for (auto &BB : F->getBasicBlockList()) {
              TerminatorInst *TI = BB.getTerminator();
              if (isa<ReturnInst>(TI)) {
                IRBuilder<> RetBuilder(TI);
                Value *Ptr = RetBuilder.CreateGEP(localTarget, idxList);
                Value *V = RetBuilder.CreateLoad(Ptr);
                RetBuilder.CreateStore(V, Elt);
              }
            }
          } else {
            // Else, copy with Builder.
            Value *Ptr = Builder.CreateGEP(localTarget, idxList);
            Value *V = Builder.CreateLoad(Ptr);
            Builder.CreateStore(V, Elt);
          }

          // Update arrayIdxList.
          for (unsigned idx=arrayLevel;idx>0;idx--) {
            arrayIdxList[idx-1]++;
            if (arrayIdxList[idx-1] < arraySizeList[idx-1])
              break;
            arrayIdxList[idx-1] = 0;
          }
        }
        // Don't override flattened SV_Target.
        if (V == Arg) {
          bSemOverride = false;
        }
        continue;
      }

      // Cannot SROA, save it to final parameter list.
      FlatParamList.emplace_back(V);
      // Create ParamAnnotation for V.
      FlatAnnotationList.emplace_back(DxilParameterAnnotation());
      DxilParameterAnnotation &flatParamAnnotation = FlatAnnotationList.back();

      flatParamAnnotation.SetParamInputQual(paramAnnotation.GetParamInputQual());
            
      flatParamAnnotation.SetInterpolationMode(annotation.GetInterpolationMode());
      flatParamAnnotation.SetSemanticString(annotation.GetSemanticString());
      flatParamAnnotation.SetCompType(annotation.GetCompType().GetKind());
      flatParamAnnotation.SetMatrixAnnotation(annotation.GetMatrixAnnotation());
      flatParamAnnotation.SetPrecise(annotation.IsPrecise());

      bool updateToRowMajor = annotation.HasMatrixAnnotation() &&
                              hasShaderInputOutput &&
                              annotation.GetMatrixAnnotation().Orientation ==
                                  MatrixOrientation::RowMajor;

      if (updateToRowMajor) {
        for (User *user : V->users()) {
          CallInst *CI = dyn_cast<CallInst>(user);
          if (!CI)
            continue;

          HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
          if (group == HLOpcodeGroup::NotHL)
            continue;
          unsigned opcode = GetHLOpcode(CI);
          unsigned rowOpcode = GetRowMajorOpcode(group, opcode);
          if (opcode == rowOpcode)
            continue;
          // Update matrix function opcode to row major version.
          Value *rowOpArg = ConstantInt::get(opcodeTy, rowOpcode);
          CI->setOperand(HLOperandIndex::kOpcodeIdx, rowOpArg);
        }
      }

      // Add debug info.
      if (DDI && V != Arg) {
        Type *Ty = V->getType();
        if (Ty->isPointerTy())
          Ty = Ty->getPointerElementType();
        unsigned size = DL.getTypeAllocSize(Ty);
        DIExpression *DDIExp = DIB.createBitPieceExpression(debugOffset, size);
        debugOffset += size;
        DIB.insertDeclare(V, DDI->getVariable(), DDIExp, DDI->getDebugLoc(), DDI);
      }

      // Flatten stream out.
      if (HLModule::IsStreamOutputPtrType(V->getType())) {
        // For stream output objects.
        // Create a value as output value.
        Type *outputType = V->getType()->getPointerElementType()->getStructElementType(0);
        Value *outputVal = Builder.CreateAlloca(outputType);
        // For each stream.Append(data)
        // transform into
        //   d = load data
        //   store outputVal, d
        //   stream.Append(outputVal)
        for (User *user : V->users()) {
          if (CallInst *CI = dyn_cast<CallInst>(user)) {
            unsigned opcode = GetHLOpcode(CI);
            if (opcode == static_cast<unsigned>(IntrinsicOp::MOP_Append)) {
              if (CI->getNumArgOperands() == (HLOperandIndex::kStreamAppendDataOpIndex + 1)) {
                Value *data =
                    CI->getArgOperand(HLOperandIndex::kStreamAppendDataOpIndex);
                DXASSERT(data->getType()->isPointerTy(),
                         "Append value must be pointer.");
                IRBuilder<> Builder(CI);

                llvm::SmallVector<llvm::Value *, 16> idxList;
                SplitCpy(data->getType(), outputVal, data, idxList,
                         /*bAllowReplace*/ false, Builder);

                CI->setArgOperand(HLOperandIndex::kStreamAppendDataOpIndex, outputVal);
              }
              else {
                // Append has been flattened.
                // Flatten store outputVal.
                // Must be struct to be flatten.
                IRBuilder<> Builder(CI);

                llvm::SmallVector<llvm::Value *, 16> idxList;
                llvm::SmallVector<llvm::Value *, 16> EltPtrList;
                // split
                SplitPtr(outputVal->getType(), outputVal, idxList, EltPtrList,
                         Builder);

                unsigned eltCount = CI->getNumArgOperands()-2;
                DXASSERT_LOCALVAR(eltCount, eltCount == EltPtrList.size(), "invalid element count");

                for (unsigned i = HLOperandIndex::kStreamAppendDataOpIndex; i < CI->getNumArgOperands(); i++) {
                  Value *DataPtr = CI->getArgOperand(i);
                  Value *EltPtr =
                      EltPtrList[i - HLOperandIndex::kStreamAppendDataOpIndex];

                  llvm::SmallVector<llvm::Value *, 16> idxList;
                  SplitCpy(DataPtr->getType(), EltPtr, DataPtr, idxList,
                           /*bAllowReplace*/ false, Builder);
                  CI->setArgOperand(i, EltPtr);
                }
              }
            }
          }
        }

        // Then split output value to generate ParamQual.
        WorkList.push_front(outputVal);
      }
    }
  }

  for (Instruction *I : deadAllocas)
    I->eraseFromParent();

  unsigned endArgIndex = FlatAnnotationList.size();
  if (bForParam && startArgIndex < endArgIndex) {
    DxilParamInputQual inputQual = paramAnnotation.GetParamInputQual();
    if (inputQual == DxilParamInputQual::OutStream0 ||
        inputQual == DxilParamInputQual::OutStream1 ||
        inputQual == DxilParamInputQual::OutStream2 ||
        inputQual == DxilParamInputQual::OutStream3)
      startArgIndex++;

    DxilParameterAnnotation &flatParamAnnotation =
        FlatAnnotationList[startArgIndex];
    const std::string &semantic = flatParamAnnotation.GetSemanticString();
    if (!semantic.empty())
      allocateSemanticIndex(FlatAnnotationList, startArgIndex,
                            semanticTypeMap);
  }

}

/// moveFunctionBlocks - Move body of F to flatF.
void SROA_Parameter_HLSL::moveFunctionBody(Function *F, Function *flatF) {
  bool updateRetType = F->getReturnType() != flatF->getReturnType();

  // Splice the body of the old function right into the new function.
  flatF->getBasicBlockList().splice(flatF->begin(), F->getBasicBlockList());

  // Update Block uses.
  if (updateRetType) {
    for (BasicBlock &BB : flatF->getBasicBlockList()) {
      if (updateRetType) {
        // Replace ret with ret void.
        if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
          // Create store for return.
          IRBuilder<> Builder(RI);
          Builder.CreateRetVoid();
          RI->eraseFromParent();
        }
      }
    }
  }
}

static void SplitArrayCopy(Value *V) {
  for (auto U = V->user_begin(); U != V->user_end();) {
    User *user = *(U++);
    if (StoreInst *ST = dyn_cast<StoreInst>(user)) {
      Value *ptr = ST->getPointerOperand();
      Value *val = ST->getValueOperand();
      IRBuilder<> Builder(ST);
      SmallVector<Value *, 16> idxList;
      SplitCpy(ptr->getType(), ptr, val, idxList, /*bAllowReplace*/ true, Builder);
      ST->eraseFromParent();
    }
  }
}

static void CheckArgUsage(Value *V, bool &bLoad, bool &bStore) {
  if (bLoad && bStore)
    return;
  for (User *user : V->users()) {
    if (LoadInst *LI = dyn_cast<LoadInst>(user)) {
      bLoad = true;
    } else if (StoreInst *SI = dyn_cast<StoreInst>(user)) {
      bStore = true;
    } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(user)) {
      CheckArgUsage(GEP, bLoad, bStore);
    } else if (CallInst *CI = dyn_cast<CallInst>(user)) {
      if (CI->getType()->isPointerTy())
        CheckArgUsage(CI, bLoad, bStore);
      else {
        HLOpcodeGroup group = GetHLOpcodeGroupByName(CI->getCalledFunction());
        if (group == HLOpcodeGroup::HLMatLoadStore) {
          HLMatLoadStoreOpcode opcode =
              static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(CI));
          switch (opcode) {
          case HLMatLoadStoreOpcode::ColMatLoad:
          case HLMatLoadStoreOpcode::RowMatLoad:
            bLoad = true;
            break;
          case HLMatLoadStoreOpcode::ColMatStore:
          case HLMatLoadStoreOpcode::RowMatStore:
            bStore = true;
            break;
          }
        }
      }
    }
  }
}
// Support store to input and load from output.
static void LegalizeDxilInputOutputs(Function *F, DxilFunctionAnnotation *EntryAnnotation) {
  BasicBlock &EntryBlk = F->getEntryBlock();
  Module *M = F->getParent();
  // Map from output to the temp created for it.
  std::unordered_map<Argument *, Value*> outputTempMap;
  for (Argument &arg : F->args()) {
    Type *Ty = arg.getType();

    DxilParameterAnnotation &paramAnnotation = EntryAnnotation->GetParameterAnnotation(arg.getArgNo());
    DxilParamInputQual qual = paramAnnotation.GetParamInputQual();

    bool isRowMajor = false;

    // Skip arg which is not a pointer.
    if (!Ty->isPointerTy()) {
      if (HLMatrixLower::IsMatrixType(Ty)) {
        // Replace matrix arg with cast to vec. It will be lowered in
        // DxilGenerationPass.
        isRowMajor = paramAnnotation.GetMatrixAnnotation().Orientation ==
                     MatrixOrientation::RowMajor;
        IRBuilder<> Builder(EntryBlk.getFirstInsertionPt());

        HLCastOpcode opcode = isRowMajor ? HLCastOpcode::RowMatrixToVecCast
                                         : HLCastOpcode::ColMatrixToVecCast;
        Value *undefVal = UndefValue::get(Ty);

        Value *Cast = HLModule::EmitHLOperationCall(
            Builder, HLOpcodeGroup::HLCast, static_cast<unsigned>(opcode), Ty,
            {undefVal}, *M);
        arg.replaceAllUsesWith(Cast);
        // Set arg as the operand.
        CallInst *CI = cast<CallInst>(Cast);
        CI->setArgOperand(HLOperandIndex::kUnaryOpSrc0Idx, &arg);
      }
      continue;
    }

    Ty = Ty->getPointerElementType();

    bool bLoad = false;
    bool bStore = false;
    CheckArgUsage(&arg, bLoad, bStore);

    bool bNeedTemp = false;
    bool bStoreInputToTemp = false;
    bool bLoadOutputFromTemp = false;

    if (qual == DxilParamInputQual::In && bStore) {
      bNeedTemp = true;
      bStoreInputToTemp = true;
    } else if (qual == DxilParamInputQual::Out && bLoad) {
      bNeedTemp = true;
      bLoadOutputFromTemp = true;
    } else if (qual == DxilParamInputQual::Inout) {
      bNeedTemp = true;
      bLoadOutputFromTemp = true;
      bStoreInputToTemp = true;
    } else if (bLoad && bStore) {
      bNeedTemp = true;
      switch (qual) {
      case DxilParamInputQual::InputPrimitive:
      case DxilParamInputQual::InputPatch:
      case DxilParamInputQual::OutputPatch:
        bStoreInputToTemp = true;
        break;
      default:
        DXASSERT(0, "invalid input qual here");
      }
    }

    if (HLMatrixLower::IsMatrixType(Ty)) {
      isRowMajor = paramAnnotation.GetMatrixAnnotation().Orientation ==
                   MatrixOrientation::RowMajor;
      bNeedTemp = true;
      if (qual == DxilParamInputQual::In)
        bStoreInputToTemp = bLoad;
      else if (qual == DxilParamInputQual::Out)
        bLoadOutputFromTemp = bStore;
      else if (qual == DxilParamInputQual::Inout) {
        bStoreInputToTemp = true;
        bLoadOutputFromTemp = true;
      }
    }

    if (bNeedTemp) {
      IRBuilder<> Builder(EntryBlk.getFirstInsertionPt());

      AllocaInst *temp = Builder.CreateAlloca(Ty);
      // Replace all uses with temp.
      arg.replaceAllUsesWith(temp);

      // Copy input to temp.
      if (bStoreInputToTemp) {
        llvm::SmallVector<llvm::Value *, 16> idxList;
        // split copy.
        SplitCpy(temp->getType(), temp, &arg, idxList, /*bAllowReplace*/ false, Builder);
        if (isRowMajor) {
          auto Iter = Builder.GetInsertPoint();
          Iter--;
          while (cast<Instruction>(Iter) != temp) {
            if (CallInst *CI = dyn_cast<CallInst>(Iter--)) {
              HLOpcodeGroup group =
                  GetHLOpcodeGroupByName(CI->getCalledFunction());
              if (group == HLOpcodeGroup::HLMatLoadStore) {
                HLMatLoadStoreOpcode opcode =
                    static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(CI));
                switch (opcode) {
                case HLMatLoadStoreOpcode::ColMatLoad: {
                  CI->setArgOperand(HLOperandIndex::kOpcodeIdx,
                                    Builder.getInt32(static_cast<unsigned>(
                                        HLMatLoadStoreOpcode::RowMatLoad)));
                } break;
                case HLMatLoadStoreOpcode::ColMatStore:
                case HLMatLoadStoreOpcode::RowMatStore:
                  CI->setArgOperand(HLOperandIndex::kOpcodeIdx,
                                    Builder.getInt32(static_cast<unsigned>(
                                        HLMatLoadStoreOpcode::RowMatStore)));
                  break;
                }
              }
            }
          }
        }
      }

      // Generate store output, temp later.
      if (bLoadOutputFromTemp) {
        outputTempMap[&arg] = temp;
      }
    }
  }

  for (BasicBlock &BB : F->getBasicBlockList()) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      IRBuilder<> Builder(RI);
      // Copy temp to output.
      for (auto It : outputTempMap) {
        Argument *output = It.first;
        Value *temp = It.second;
        llvm::SmallVector<llvm::Value *, 16> idxList;

        DxilParameterAnnotation &paramAnnotation =
            EntryAnnotation->GetParameterAnnotation(output->getArgNo());
        bool hasMatrix = paramAnnotation.HasMatrixAnnotation();
        bool isRowMajor = false;
        if (hasMatrix) {
          isRowMajor = paramAnnotation.GetMatrixAnnotation().Orientation ==
                       MatrixOrientation::RowMajor;
        }

        auto Iter = Builder.GetInsertPoint();
        bool onlyRetBlk = false;
        if (RI != BB.begin())
          Iter--;
        else
          onlyRetBlk = true;
        // split copy.
        SplitCpy(output->getType(), output, temp, idxList,
                 /*bAllowReplace*/ false, Builder);
        if (isRowMajor) {
          if (onlyRetBlk)
            Iter = BB.begin();

          while (cast<Instruction>(Iter) != RI) {
            if (CallInst *CI = dyn_cast<CallInst>(++Iter)) {
              HLOpcodeGroup group =
                  GetHLOpcodeGroupByName(CI->getCalledFunction());
              if (group == HLOpcodeGroup::HLMatLoadStore) {
                HLMatLoadStoreOpcode opcode =
                    static_cast<HLMatLoadStoreOpcode>(GetHLOpcode(CI));
                switch (opcode) {
                case HLMatLoadStoreOpcode::ColMatLoad: {
                  CI->setArgOperand(HLOperandIndex::kOpcodeIdx,
                                    Builder.getInt32(static_cast<unsigned>(
                                        HLMatLoadStoreOpcode::RowMatLoad)));
                } break;
                case HLMatLoadStoreOpcode::ColMatStore:
                case HLMatLoadStoreOpcode::RowMatStore:
                  CI->setArgOperand(HLOperandIndex::kOpcodeIdx,
                                    Builder.getInt32(static_cast<unsigned>(
                                        HLMatLoadStoreOpcode::RowMatStore)));
                  break;
                }
              }
            }
          }
        }
      }
      // Clone the return.
      Builder.CreateRet(RI->getReturnValue());
      RI->eraseFromParent();
    }
  }
}

void SROA_Parameter_HLSL::createFlattenedFunction(Function *F) {
  // Change memcpy into ld/st first
  MemcpySplitter splitter(F->getContext());
  splitter.Split(*F);

  // Skip void (void) function.
  if (F->getReturnType()->isVoidTy() && F->getArgumentList().empty()) {
    return;
  }

  DxilFunctionAnnotation *funcAnnotation = m_pHLModule->GetFunctionAnnotation(F);
  DXASSERT(funcAnnotation, "must find annotation for function");

  std::deque<Value *> WorkList;
  
  std::vector<Value *> FlatParamList;
  std::vector<DxilParameterAnnotation> FlatParamAnnotationList;
  // Add all argument to worklist.
  for (Argument &Arg : F->args()) {
    // merge GEP use for arg.
    MergeGepUse(&Arg);
    // Insert point may be removed. So recreate builder every time.
    IRBuilder<> Builder(F->getEntryBlock().getFirstInsertionPt());
    DxilParameterAnnotation &paramAnnotation =
        funcAnnotation->GetParameterAnnotation(Arg.getArgNo());
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(&Arg);
    flattenArgument(F, &Arg, paramAnnotation, FlatParamList,
                    FlatParamAnnotationList, Builder, DDI);
  }

  Type *retType = F->getReturnType();
  std::vector<Value *> FlatRetList;
  std::vector<DxilParameterAnnotation> FlatRetAnnotationList;
  // Split and change to out parameter.
  if (!retType->isVoidTy()) {
    Instruction *InsertPt = F->getEntryBlock().getFirstInsertionPt();
    IRBuilder<> Builder(InsertPt);
    Value *retValAddr = Builder.CreateAlloca(retType);
    // Create DbgDecl for the ret value.
    if (DISubprogram *funcDI = getDISubprogram(F)) {
       DITypeRef RetDITyRef = funcDI->getType()->getTypeArray()[0];
       DITypeIdentifierMap EmptyMap;
       DIType * RetDIType = RetDITyRef.resolve(EmptyMap);
       DIBuilder DIB(*F->getParent(), /*AllowUnresolved*/ false);
       DILocalVariable *RetVar = DIB.createLocalVariable(llvm::dwarf::Tag::DW_TAG_arg_variable, funcDI, F->getName().str() + ".Ret", funcDI->getFile(),
           funcDI->getLine(), RetDIType);
       DIExpression *Expr = nullptr;
       // TODO: how to get col?
       DILocation *DL = DILocation::get(F->getContext(), funcDI->getLine(), 0,  funcDI);
       DIB.insertDeclare(retValAddr, RetVar, Expr, DL, Builder.GetInsertPoint());
    }
    for (BasicBlock &BB : F->getBasicBlockList()) {
      if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
        // Create store for return.
        IRBuilder<> RetBuilder(RI);
        RetBuilder.CreateStore(RI->getReturnValue(), retValAddr);
        // Clone the return.
        ReturnInst *NewRet = RetBuilder.CreateRet(RI->getReturnValue());
        if (RI == InsertPt) {
          Builder.SetInsertPoint(NewRet);
        }
        RI->eraseFromParent();
      }
    }
    // Create a fake store to keep retValAddr so it can be flattened.
    if (retValAddr->user_empty()) {
      Builder.CreateStore(UndefValue::get(retType), retValAddr);
    }

    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(retValAddr);
    flattenArgument(F, retValAddr, funcAnnotation->GetRetTypeAnnotation(),
                    FlatRetList, FlatRetAnnotationList, Builder, DDI);
  }

  // Always change return type as parameter.
  // By doing this, no need to check return when generate storeOutput.
  if (FlatRetList.size() ||
      // For empty struct return type.
      !retType->isVoidTy()) {
    // Return value is flattened.
    // Change return value into out parameter.
    retType = Type::getVoidTy(retType->getContext());
    // Merge return data info param data.
    FlatParamList.insert(FlatParamList.end(), FlatRetList.begin(), FlatRetList.end());

    FlatParamAnnotationList.insert(FlatParamAnnotationList.end(),
                                   FlatRetAnnotationList.begin(),
                                   FlatRetAnnotationList.end());
  }

  // TODO: take care AttributeSet for function and each argument.

  std::vector<Type *> FinalTypeList;
  for (Value * arg : FlatParamList) {
    FinalTypeList.emplace_back(arg->getType());
  }

  unsigned extraParamSize = 0;
  if (m_pHLModule->HasHLFunctionProps(F)) {
    HLFunctionProps &funcProps = m_pHLModule->GetHLFunctionProps(F);
    if (funcProps.shaderKind == ShaderModel::Kind::Vertex) {
      auto &VS = funcProps.ShaderProps.VS;
      Type *outFloatTy = Type::getFloatPtrTy(F->getContext());
      // Add out float parameter for each clip plane.
      unsigned i=0;
      for (; i < DXIL::kNumClipPlanes; i++) {
        if (!VS.clipPlanes[i])
          break;
        FinalTypeList.emplace_back(outFloatTy);
      }
      extraParamSize = i;
    }
  }

  FunctionType *flatFuncTy = FunctionType::get(retType, FinalTypeList, false);
  // Return if nothing changed.
  if (flatFuncTy == F->getFunctionType()) {
    // Copy semantic allocation.
    if (!FlatParamAnnotationList.empty()) {
      if (!FlatParamAnnotationList[0].GetSemanticString().empty()) {
        for (unsigned i = 0; i < FlatParamAnnotationList.size(); i++) {
          DxilParameterAnnotation &paramAnnotation = funcAnnotation->GetParameterAnnotation(i);
          DxilParameterAnnotation &flatParamAnnotation = FlatParamAnnotationList[i];
          paramAnnotation.SetSemanticIndexVec(flatParamAnnotation.GetSemanticIndexVec());
          paramAnnotation.SetSemanticString(flatParamAnnotation.GetSemanticString());
        }
      }
    }
    // Support store to input and load from output.
    LegalizeDxilInputOutputs(F, funcAnnotation);
    return;
  }

  std::string flatName = F->getName().str() + ".flat";
  DXASSERT(nullptr == F->getParent()->getFunction(flatName),
           "else overwriting existing function");
  Function *flatF =
      cast<Function>(F->getParent()->getOrInsertFunction(flatName, flatFuncTy));
  funcMap[F] = flatF;

  // Update function debug info.
  if (DISubprogram *funcDI = getDISubprogram(F))
    funcDI->replaceFunction(flatF);

  // Create FunctionAnnotation for flatF.
  DxilFunctionAnnotation *flatFuncAnnotation = m_pHLModule->AddFunctionAnnotation(flatF);
  
  // Don't need to set Ret Info, flatF always return void now.

  // Param Info
  for (unsigned ArgNo = 0; ArgNo < FlatParamAnnotationList.size(); ++ArgNo) {
    DxilParameterAnnotation &paramAnnotation = flatFuncAnnotation->GetParameterAnnotation(ArgNo);
    paramAnnotation = FlatParamAnnotationList[ArgNo];    
  }
  DXASSERT(flatF->arg_size() == (extraParamSize + FlatParamAnnotationList.size()), "parameter count mismatch");
  // ShaderProps.
  if (m_pHLModule->HasHLFunctionProps(F)) {
    HLFunctionProps &funcProps = m_pHLModule->GetHLFunctionProps(F);
    std::unique_ptr<HLFunctionProps> flatFuncProps = std::make_unique<HLFunctionProps>();
    flatFuncProps->shaderKind = funcProps.shaderKind;
    flatFuncProps->ShaderProps = funcProps.ShaderProps;
    m_pHLModule->AddHLFunctionProps(flatF, flatFuncProps);
    if (funcProps.shaderKind == ShaderModel::Kind::Vertex) {
      auto &VS = funcProps.ShaderProps.VS;
      unsigned clipArgIndex = FlatParamAnnotationList.size();
      // Add out float SV_ClipDistance for each clip plane.
      for (unsigned i = 0; i < DXIL::kNumClipPlanes; i++) {
        if (!VS.clipPlanes[i])
          break;
        DxilParameterAnnotation &paramAnnotation =
            flatFuncAnnotation->GetParameterAnnotation(clipArgIndex+i);
        paramAnnotation.SetParamInputQual(DxilParamInputQual::Out);
        Twine semName = Twine("SV_ClipDistance") + Twine(i);
        paramAnnotation.SetSemanticString(semName.str());
        paramAnnotation.SetCompType(DXIL::ComponentType::F32);
        paramAnnotation.AppendSemanticIndex(i);
      }
    }
  }

  // Move function body into flatF.
  moveFunctionBody(F, flatF);

  // Replace old parameters with flatF Arguments.
  auto argIter = flatF->arg_begin();
  auto flatArgIter = FlatParamList.begin();
  LLVMContext &Context = F->getContext();

  while (argIter != flatF->arg_end()) {
    Argument *Arg = argIter++;
    if (flatArgIter == FlatParamList.end()) {
      DXASSERT(extraParamSize>0, "parameter count mismatch");
      break;
    }
    Value *flatArg = *(flatArgIter++);

    flatArg->replaceAllUsesWith(Arg);
    // Update arg debug info.
    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(flatArg);
    if (DDI) {
      Value *VMD = MetadataAsValue::get(Context, ValueAsMetadata::get(Arg));
      DDI->setArgOperand(0, VMD);
    }

    MergeGepUse(Arg);
    // Flatten store of array parameter.
    if (Arg->getType()->isPointerTy()) {
      Type *Ty = Arg->getType()->getPointerElementType();
      if (Ty->isArrayTy())
        SplitArrayCopy(Arg);
    }
  }
  // Support store to input and load from output.
  LegalizeDxilInputOutputs(flatF, flatFuncAnnotation);
}

void SROA_Parameter_HLSL::createFlattenedFunctionCall(Function *F, Function *flatF, CallInst *CI) {
  DxilFunctionAnnotation *funcAnnotation = m_pHLModule->GetFunctionAnnotation(F);
  DXASSERT(funcAnnotation, "must find annotation for function");

  std::vector<Value *> FlatParamList;
  std::vector<DxilParameterAnnotation> FlatParamAnnotationList;

  IRBuilder<> AllocaBuilder(
      CI->getParent()->getParent()->getEntryBlock().getFirstInsertionPt());
  IRBuilder<> CallBuilder(CI);
  IRBuilder<> RetBuilder(CI->getNextNode());

  Type *retType = F->getReturnType();
  std::vector<Value *> FlatRetList;
  std::vector<DxilParameterAnnotation> FlatRetAnnotationList;
  // Split and change to out parameter.
  if (!retType->isVoidTy()) {
    Value *retValAddr = AllocaBuilder.CreateAlloca(retType);
    // Create DbgDecl for the ret value.
    if (DISubprogram *funcDI = getDISubprogram(F)) {
       DITypeRef RetDITyRef = funcDI->getType()->getTypeArray()[0];
       DITypeIdentifierMap EmptyMap;
       DIType * RetDIType = RetDITyRef.resolve(EmptyMap);
       DIBuilder DIB(*F->getParent(), /*AllowUnresolved*/ false);
       DILocalVariable *RetVar = DIB.createLocalVariable(llvm::dwarf::Tag::DW_TAG_arg_variable, funcDI, F->getName().str() + ".Ret", funcDI->getFile(),
           funcDI->getLine(), RetDIType);
       DIExpression *Expr = nullptr;
       // TODO: how to get col?
       DILocation *DL = DILocation::get(F->getContext(), funcDI->getLine(), 0,  funcDI);
       DIB.insertDeclare(retValAddr, RetVar, Expr, DL, CI);
    }

    // Load ret value and replace CI.
    Value *newRetVal = RetBuilder.CreateLoad(retValAddr);
    CI->replaceAllUsesWith(newRetVal);
    // Flat ret val
    flattenArgument(flatF, retValAddr, funcAnnotation->GetRetTypeAnnotation(),
                    FlatRetList, FlatRetAnnotationList, AllocaBuilder,
                    /*DbgDeclareInst*/nullptr);
  }

  std::vector<Value *> args;
  for (auto &arg : CI->arg_operands()) {
    args.emplace_back(arg.get());
  }
  // Remove CI from user of args.
  CI->dropAllReferences();

  // Add all argument to worklist.
  for (unsigned i=0;i<args.size();i++) {
    DxilParameterAnnotation &paramAnnotation =
        funcAnnotation->GetParameterAnnotation(i);
    Value *arg = args[i];
    if (arg->getType()->isPointerTy()) {
      // For pointer, alloca another pointer, replace in CI.
      Value *tempArg =
          AllocaBuilder.CreateAlloca(arg->getType()->getPointerElementType());

      DxilParamInputQual inputQual = paramAnnotation.GetParamInputQual();
      // TODO: support special InputQual like InputPatch.
      if (inputQual == DxilParamInputQual::In ||
          inputQual == DxilParamInputQual::Inout) {
        // Copy in param.
        Value *v = CallBuilder.CreateLoad(arg);
        CallBuilder.CreateStore(v, tempArg);
      }

      if (inputQual == DxilParamInputQual::Out ||
          inputQual == DxilParamInputQual::Inout) {
        // Copy out param.
        Value *v = RetBuilder.CreateLoad(tempArg);
        RetBuilder.CreateStore(v, arg);
      }
      arg = tempArg;
      flattenArgument(flatF, arg, paramAnnotation, FlatParamList,
                      FlatParamAnnotationList, AllocaBuilder,
                      /*DbgDeclareInst*/ nullptr);
    } else {
      // Cannot SROA, save it to final parameter list.
      FlatParamList.emplace_back(arg);
      // Create ParamAnnotation for V.
      FlatRetAnnotationList.emplace_back(DxilParameterAnnotation());
      DxilParameterAnnotation &flatParamAnnotation =
          FlatRetAnnotationList.back();
      flatParamAnnotation = paramAnnotation;
    }
  }

  // Always change return type as parameter.
  // By doing this, no need to check return when generate storeOutput.
  if (FlatRetList.size() ||
      // For empty struct return type.
      !retType->isVoidTy()) {
    // Merge return data info param data.
    FlatParamList.insert(FlatParamList.end(), FlatRetList.begin(), FlatRetList.end());

    FlatParamAnnotationList.insert(FlatParamAnnotationList.end(),
                                   FlatRetAnnotationList.begin(),
                                   FlatRetAnnotationList.end());
  }
  CallInst *NewCI = CallBuilder.CreateCall(flatF, FlatParamList);

  CallBuilder.SetInsertPoint(NewCI);

  CI->eraseFromParent();
}

void SROA_Parameter_HLSL::replaceCall(Function *F, Function *flatF) {
  // Update entry function.
  if (F == m_pHLModule->GetEntryFunction()) {
    m_pHLModule->SetEntryFunction(flatF);
  }
  // Update patch constant function.
  if (m_pHLModule->HasHLFunctionProps(flatF)) {
    HLFunctionProps &funcProps = m_pHLModule->GetHLFunctionProps(flatF);
    if (funcProps.shaderKind == DXIL::ShaderKind::Hull) {
      Function *oldPatchConstantFunc =
          funcProps.ShaderProps.HS.patchConstantFunc;
      if (funcMap.count(oldPatchConstantFunc))
        funcProps.ShaderProps.HS.patchConstantFunc =
            funcMap[oldPatchConstantFunc];
    }
  }
  // TODO: flatten vector argument and lower resource argument when flatten
  // functions.
  for (auto it = F->user_begin(); it != F->user_end(); ) {
    CallInst *CI = cast<CallInst>(*(it++));
    createFlattenedFunctionCall(F, flatF, CI);
  }
}

// Public interface to the SROA_Parameter_HLSL pass
ModulePass *llvm::createSROA_Parameter_HLSL() {
  return new SROA_Parameter_HLSL();
}

//===----------------------------------------------------------------------===//
// DynamicIndexingVector to Array.
//===----------------------------------------------------------------------===//

namespace {
class DynamicIndexingVectorToArray : public ModulePass {
  bool ReplaceAllVectors;
  bool runOnFunction(Function &F);
  void runOnInternalGlobal(GlobalVariable *GV, HLModule *HLM);
  bool m_HasDbgInfo;
public:
  explicit DynamicIndexingVectorToArray(bool ReplaceAll = false)
      : ModulePass(ID), ReplaceAllVectors(ReplaceAll) {}
  static char ID; // Pass identification, replacement for typeid
  bool runOnModule(Module &M) override;
};
}

static bool HasVectorDynamicIndexing(Value *V) {
  for (auto User : V->users()) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      for (auto Idx = GEP->idx_begin(); Idx != GEP->idx_end(); ++Idx) {
        if (!isa<ConstantInt>(Idx))
          return true;
      }
    }
  }
  return false;
}

static void ReplaceStaticIndexingOnVector(Value *V) {
  for (auto U = V->user_begin(), E = V->user_end(); U != E;) {
    Value *User = *(U++);
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      // Only work on element access for vector.
      if (GEP->getNumOperands() == 3) {
        auto Idx = GEP->idx_begin();
        // Skip the pointer idx.
        Idx++;
        ConstantInt *constIdx = cast<ConstantInt>(Idx);

        for (auto GEPU = GEP->user_begin(), GEPE = GEP->user_end();
             GEPU != GEPE;) {
          Instruction *GEPUser = cast<Instruction>(*(GEPU++));

          IRBuilder<> Builder(GEPUser);

          if (LoadInst *ldInst = dyn_cast<LoadInst>(GEPUser)) {
            // Change
            //    ld a->x
            // into
            //    b = ld a
            //    b.x
            Value *ldVal = Builder.CreateLoad(V);
            Value *Elt = Builder.CreateExtractElement(ldVal, constIdx);
            ldInst->replaceAllUsesWith(Elt);
            ldInst->eraseFromParent();
          } else {
            // Change
            //    st val, a->x
            // into
            //    tmp = ld a
            //    tmp.x = val
            //    st tmp, a
            // Must be store inst here.
            StoreInst *stInst = cast<StoreInst>(GEPUser);
            Value *val = stInst->getValueOperand();
            Value *ldVal = Builder.CreateLoad(V);
            ldVal = Builder.CreateInsertElement(ldVal, val, constIdx);
            Builder.CreateStore(ldVal, V);
            stInst->eraseFromParent();
          }
        }
        GEP->eraseFromParent();
      }
    }
  }
}

// Replace vector with array.
static void VectorToArray(Value *V, Value *A) {
  Type *i32Ty = Type::getInt32Ty(V->getContext());
  unsigned size = V->getType()->getPointerElementType()->getVectorNumElements();
  std::vector<CallInst *> callUsers;
  std::vector<AllocaInst *> callUserTempAllocas;

  for (auto U = V->user_begin(); U != V->user_end();) {
    User *User = (*U++);
    if (isa<ConstantExpr>(User)) {
      if (User->user_empty())
        continue;
      if (GEPOperator *GEP = dyn_cast<GEPOperator>(User)) {
        SmallVector<Value *, 4> idxList(GEP->idx_begin(), GEP->idx_end());
        IRBuilder<> Builder(i32Ty->getContext());
        Value *newGEP = Builder.CreateGEP(A, idxList);
        if (GEP->getType()->getPointerElementType()->isVectorTy()) {
          VectorToArray(GEP, newGEP);
        } else {
          GEP->replaceAllUsesWith(newGEP);
        }
        continue;
      }
    }
    Instruction *UserInst = cast<Instruction>(User);
    IRBuilder<> Builder(UserInst);
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      SmallVector<Value *, 4> idxList(GEP->idx_begin(), GEP->idx_end());
      Value *newGEP = Builder.CreateGEP(A, idxList);
      if (GEP->getType()->getPointerElementType()->isVectorTy()) {
        VectorToArray(GEP, newGEP);
      } else {
        GEP->replaceAllUsesWith(newGEP);
      }
      GEP->eraseFromParent();
    } else if (LoadInst *ldInst = dyn_cast<LoadInst>(User)) {
      // If ld whole struct, need to split the load.
      Value *newLd = UndefValue::get(ldInst->getType());
      Value *zero = ConstantInt::get(i32Ty, 0);
      for (unsigned i = 0; i < size; i++) {
        Value *idx = ConstantInt::get(i32Ty, i);
        Value *GEP = Builder.CreateInBoundsGEP(A, {zero, idx});
        Value *Elt = Builder.CreateLoad(GEP);
        newLd = Builder.CreateInsertElement(newLd, Elt, i);
      }
      ldInst->replaceAllUsesWith(newLd);
      ldInst->eraseFromParent();
    } else if (StoreInst *stInst = dyn_cast<StoreInst>(User)) {
      Value *val = stInst->getValueOperand();
      Value *zero = ConstantInt::get(i32Ty, 0);
      for (unsigned i = 0; i < size; i++) {
        Value *Elt = Builder.CreateExtractElement(val, i);
        Value *idx = ConstantInt::get(i32Ty, i);
        Value *GEP = Builder.CreateInBoundsGEP(A, {zero, idx});
        Builder.CreateStore(Elt, GEP);
      }
      stInst->eraseFromParent();
    } else if (CallInst *CI = dyn_cast<CallInst>(User)) {
      // Create alloca at the beginning of parent function.
      IRBuilder<> allocaBuilder(CI->getParent()->getParent()->begin()->begin());
      AllocaInst *AI = allocaBuilder.CreateAlloca(V->getType()->getPointerElementType());
      callUsers.emplace_back(CI);
      callUserTempAllocas.emplace_back(AI);
    } else {
      DXASSERT(0, "not implement yet");
    }
  }

  for (unsigned i = 0; i < callUsers.size(); i++) {
    CallInst *CI = callUsers[i];
    AllocaInst *AI = callUserTempAllocas[i];
    IRBuilder<> Builder(CI);
    // Copy data to AI before CI.
    Value *newLd = UndefValue::get(AI->getAllocatedType());
    Value *zero = ConstantInt::get(i32Ty, 0);
    for (unsigned i = 0; i < size; i++) {
      Value *idx = ConstantInt::get(i32Ty, i);
      Value *GEP = Builder.CreateInBoundsGEP(A, {zero, idx});
      Value *Elt = Builder.CreateLoad(GEP);
      newLd = Builder.CreateInsertElement(newLd, Elt, i);
    }
    Builder.CreateStore(newLd, AI);
    CI->replaceUsesOfWith(V, AI);
    // Copy back data from AI to Array after CI.
    Builder.SetInsertPoint(CI->getNextNode());
    Value *result = Builder.CreateLoad(AI);
    for (unsigned i = 0; i < size; i++) {
      Value *Elt = Builder.CreateExtractElement(result, i);
      Value *idx = ConstantInt::get(i32Ty, i);
      Value *GEP = Builder.CreateInBoundsGEP(A, {zero, idx});
      Builder.CreateStore(Elt, GEP);
    }
  }
}

static Value *GenerateVectorFromArrayLoad(VectorType *DstTy, Value *A,
                                          IRBuilder<> &Builder) {
  Type *i32Ty = Type::getInt32Ty(DstTy->getContext());
  Value *zero = ConstantInt::get(i32Ty, 0);
  Value *VecVal = UndefValue::get(DstTy);
  for (unsigned ai = 0; ai < DstTy->getNumElements(); ai++) {
    Value *EltGEP =
        Builder.CreateInBoundsGEP(A, {zero, ConstantInt::get(i32Ty, ai)});
    Value *arrayElt = Builder.CreateLoad(EltGEP);
    VecVal = Builder.CreateInsertElement(VecVal, arrayElt, ai);
  }
  return VecVal;
}

static Value *GenerateArrayLoad(Type *DstTy, Value *A, IRBuilder<> &Builder) {
  ArrayType *AT = cast<ArrayType>(DstTy);
  Type *EltTy = AT->getElementType();
  Type *i32Ty = Type::getInt32Ty(EltTy->getContext());
  Value *zero = ConstantInt::get(i32Ty, 0);
  if (EltTy->isArrayTy()) {
    Value *arrayVal = UndefValue::get(AT);
    for (unsigned ai = 0; ai < AT->getNumElements(); ai++) {
      Value *EltGEP = Builder.CreateInBoundsGEP(A, {zero, ConstantInt::get(i32Ty, ai)});
      Value *arrayElt = GenerateArrayLoad(EltTy, EltGEP, Builder);
      arrayVal = Builder.CreateInsertValue(arrayVal, arrayElt, ai);
    }
    return arrayVal;
  } else {
    // Generate vector here.
    VectorType *VT = cast<VectorType>(EltTy);
    Value *arrayVal = UndefValue::get(AT);
    for (unsigned ai = 0; ai < AT->getNumElements(); ai++) {
      Value *EltGEP = Builder.CreateInBoundsGEP(A, {zero, ConstantInt::get(i32Ty, ai)});
      Value *arrayElt = GenerateVectorFromArrayLoad(VT, EltGEP, Builder);
      arrayVal = Builder.CreateInsertValue(arrayVal, arrayElt, ai);
    }
    return arrayVal;
  }
}

static void GenerateArrayStore(Value *VecVal, Value *scalarArrayPtr,
                               IRBuilder<> &Builder) {
  Value *zero = Builder.getInt32(0);
  if (ArrayType *AT = dyn_cast<ArrayType>(VecVal->getType())) {
    for (unsigned ai = 0; ai < AT->getNumElements(); ai++) {
      Value *EltGEP = Builder.CreateInBoundsGEP(scalarArrayPtr,
                                                {zero, Builder.getInt32(ai)});
      Value *EltVal = Builder.CreateExtractValue(VecVal, ai);
      GenerateArrayStore(EltVal, EltGEP, Builder);
    }
  } else {
    // Generate vector here.
    VectorType *VT = cast<VectorType>(VecVal->getType());
    for (unsigned ai = 0; ai < VT->getNumElements(); ai++) {
      Value *EltGEP = Builder.CreateInBoundsGEP(scalarArrayPtr,
                                                {zero, Builder.getInt32(ai)});

      Value *Elt = Builder.CreateExtractElement(VecVal, ai);
      Builder.CreateStore(Elt, EltGEP);
    }
  }
}
// Replace vector array with array.
static void VectorArrayToArray(Value *VA, Value *A) {

  for (auto U = VA->user_begin(); U != VA->user_end();) {
    User *User = *(U++);
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(User)) {
      IRBuilder<> Builder(GEP);
      SmallVector<Value *, 4> idxList(GEP->idx_begin(), GEP->idx_end());
      Value *newGEP = Builder.CreateGEP(A, idxList);
      Type *Ty = GEP->getType()->getPointerElementType();
      if (Ty->isVectorTy()) {
        VectorToArray(GEP, newGEP);
      } else if (Ty->isArrayTy()) {
        VectorArrayToArray(GEP, newGEP);
      } else {
        assert(Ty->isSingleValueType() && "must be vector subscript here");
        GEP->replaceAllUsesWith(newGEP);
      }
      GEP->eraseFromParent();
    } else if (GEPOperator *GEPOp = dyn_cast<GEPOperator>(User)) {
      IRBuilder<> Builder(GEPOp->getContext());
      SmallVector<Value *, 4> idxList(GEPOp->idx_begin(), GEPOp->idx_end());
      Value *newGEP = Builder.CreateGEP(A, idxList);
      Type *Ty = GEPOp->getType()->getPointerElementType();
      if (Ty->isVectorTy()) {
        VectorToArray(GEPOp, newGEP);
      } else if (Ty->isArrayTy()) {
        VectorArrayToArray(GEPOp, newGEP);
      } else {
        assert(Ty->isSingleValueType() && "must be vector subscript here");
        GEPOp->replaceAllUsesWith(newGEP);
      }
    } else if (LoadInst *ldInst = dyn_cast<LoadInst>(User)) {
      IRBuilder<> Builder(ldInst);
      Value *arrayLd = GenerateArrayLoad(ldInst->getType(), A, Builder);
      ldInst->replaceAllUsesWith(arrayLd);
      ldInst->eraseFromParent();
    } else if (StoreInst *stInst = dyn_cast<StoreInst>(User)) {
      IRBuilder<> Builder(stInst);
      GenerateArrayStore(stInst->getValueOperand(), A, Builder);
      stInst->eraseFromParent();
    } else {
      assert(0 && "not implement yet");
    }
  }
}

static void flatArrayLoad(LoadInst *LI) {
  ArrayType *AT = cast<ArrayType>(LI->getType());
  unsigned size = AT->getArrayNumElements();
  Value *Ptr = LI->getPointerOperand();
  std::vector<LoadInst *> elements(size);
  Value *Result = UndefValue::get(AT);
  IRBuilder<> Builder(LI);
  Value *zero = Builder.getInt32(0);
  for (unsigned i=0;i<size;i++) {
    Value *EltPtr = Builder.CreateInBoundsGEP(Ptr, {zero, Builder.getInt32(i)});
    LoadInst *Elt = Builder.CreateLoad(EltPtr);
    elements[i] = Elt;
    Result = Builder.CreateInsertValue(Result, Elt, i);
  }

  Type *EltTy = AT->getElementType();
  if (isa<ArrayType>(EltTy)) {
    for (unsigned i = 0; i < size; i++) {
      LoadInst *Elt = elements[i];
      flatArrayLoad(Elt);
    }
  }
  LI->replaceAllUsesWith(Result);
  LI->eraseFromParent();
}

static void flatArrayStore(StoreInst *SI) {
  Value *V = SI->getValueOperand();
  ArrayType *AT = cast<ArrayType>(V->getType());
  unsigned size = AT->getArrayNumElements();
  Value *Ptr = SI->getPointerOperand();
  std::vector<StoreInst *> elements(size);

  IRBuilder<> Builder(SI);
  Value *zero = Builder.getInt32(0);
  for (unsigned i=0;i<size;i++) {
    Value *Elt = Builder.CreateExtractValue(V, i);
    Value *EltPtr = Builder.CreateInBoundsGEP(Ptr, {zero, Builder.getInt32(i)});
    StoreInst *EltSt = Builder.CreateStore(Elt, EltPtr);
    elements[i] = EltSt;
  }

  Type *EltTy = AT->getElementType();
  if (isa<ArrayType>(EltTy)) {
    for (unsigned i = 0; i < size; i++) {
      StoreInst *Elt = elements[i];
      flatArrayStore(Elt);
    }
  }
  SI->eraseFromParent();
}

bool DynamicIndexingVectorToArray::runOnFunction(Function &F) {
  std::vector<AllocaInst *> workList;
  std::vector<AllocaInst *> arrayLdStWorkList;
  std::vector<Type *> targetTypeList;
  // Scan the entry basic block, adding allocas to the worklist.
  BasicBlock &BB = F.getEntryBlock();
  for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
    if (!isa<AllocaInst>(I))
      continue;
    AllocaInst *A = cast<AllocaInst>(I);
    Type *Ty = A->getAllocatedType();
    if (Ty->isVectorTy()) {
      if (ReplaceAllVectors || HasVectorDynamicIndexing(A)) {
        workList.emplace_back(A);
        Type *vecAT = ArrayType::get(Ty->getVectorElementType(),
                                     Ty->getVectorNumElements());
        targetTypeList.emplace_back(vecAT);
      } else {
        ReplaceStaticIndexingOnVector(A);
      }
    } else if (Ty->isArrayTy()) {
      SmallVector<ArrayType *, 4> nestArrayTys;
      ArrayType *AT = cast<ArrayType>(Ty);
      nestArrayTys.emplace_back(AT);

      Type *EltTy = AT->getElementType();
      // support multi level of array
      while (EltTy->isArrayTy()) {
        ArrayType *ElAT = cast<ArrayType>(EltTy);
        nestArrayTys.emplace_back(ElAT);
        EltTy = ElAT->getElementType();
      }
      // Array must be replaced even without dynamic indexing to remove vector
      // type in dxil.
      // TODO: optimize static array index in later pass.
      if (EltTy->isVectorTy()) {
        workList.emplace_back(A);
        Type *vecAT = ArrayType::get(EltTy->getVectorElementType(),
                                     EltTy->getVectorNumElements());
        ArrayType *AT = CreateNestArrayTy(vecAT, nestArrayTys);
        targetTypeList.emplace_back(AT);
      } else {
        for (User *U : A->users()) {
          if (isa<LoadInst>(U) || isa<StoreInst>(U)) {
            arrayLdStWorkList.emplace_back(A);
            break;
          }
        }
      }
    }
  }
  // Flat array load store.
  for (AllocaInst *A : arrayLdStWorkList) {
    for (auto It = A->user_begin(); It != A->user_end(); ) {
      User *U = *(It++);
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        flatArrayLoad(LI);
      } else if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
        flatArrayStore(SI);
      }
    }
  }

  LLVMContext &Context = F.getContext();

  unsigned size = workList.size();
  for (unsigned i = 0; i < size; i++) {
    AllocaInst *V = workList[i];
    Type *Ty = targetTypeList[i];
    IRBuilder<> Builder(V);

    AllocaInst *A = Builder.CreateAlloca(Ty, nullptr, V->getName());

    if (HLModule::HasPreciseAttributeWithMetadata(V))
      HLModule::MarkPreciseAttributeWithMetadata(A);

    DbgDeclareInst *DDI = llvm::FindAllocaDbgDeclare(V);
    if (DDI) {
      Value *DDIVar = MetadataAsValue::get(Context, DDI->getRawVariable());
      Value *DDIExp = MetadataAsValue::get(Context, DDI->getRawExpression());
      Value *VMD = MetadataAsValue::get(Context, ValueAsMetadata::get(A));
      IRBuilder<> debugBuilder(DDI);
      debugBuilder.CreateCall(DDI->getCalledFunction(), {VMD, DDIVar, DDIExp});
    }

    if (V->getAllocatedType()->isVectorTy())
      VectorToArray(V, A);
    else
      VectorArrayToArray(V, A);

    V->eraseFromParent();
  }

  return size > 0;
}

void DynamicIndexingVectorToArray::runOnInternalGlobal(GlobalVariable *GV,
                                                       HLModule *HLM) {
  Type *Ty = GV->getType()->getPointerElementType();
  // Convert vector type to array type.
  if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
    SmallVector<unsigned, 4> arraySizeList;
    while (Ty->isArrayTy()) {
      arraySizeList.push_back(Ty->getArrayNumElements());
      Ty = Ty->getArrayElementType();
    }

    DXASSERT_NOMSG(Ty->isVectorTy());
    Ty = ArrayType::get(Ty->getVectorElementType(), Ty->getVectorNumElements());

    for (auto arraySize = arraySizeList.rbegin();
         arraySize != arraySizeList.rend(); arraySize++)
      Ty = ArrayType::get(Ty, *arraySize);
  } else {
    DXASSERT_NOMSG(Ty->isVectorTy());
    Ty = ArrayType::get(Ty->getVectorElementType(), Ty->getVectorNumElements());
  }

  ArrayType *AT = cast<ArrayType>(Ty);
  // So set init val to undef.
  Constant *InitVal = UndefValue::get(AT);
  if (GV->hasInitializer()) {
    Constant *vecInitVal = GV->getInitializer();
    if (isa<ConstantAggregateZero>(vecInitVal))
      InitVal = ConstantAggregateZero::get(AT);
    else if (!isa<UndefValue>(vecInitVal)) {
      // build arrayInitVal.
      // Only vector initializer could reach here.
      // Complex case will use store to init.
      DXASSERT_NOMSG(vecInitVal->getType()->isVectorTy());
      ConstantDataVector *CDV = cast<ConstantDataVector>(vecInitVal);
      unsigned vecSize = CDV->getType()->getVectorNumElements();
      std::vector<Constant *> vals;
      for (unsigned i = 0; i < vecSize; i++)
        vals.emplace_back(CDV->getAggregateElement(i));
      InitVal = ConstantArray::get(AT, vals);
    }
  }

  bool isConst = GV->isConstant();
  GlobalVariable::ThreadLocalMode TLMode = GV->getThreadLocalMode();
  unsigned AddressSpace = GV->getType()->getAddressSpace();
  GlobalValue::LinkageTypes linkage = GV->getLinkage();

  Module *M = GV->getParent();
  GlobalVariable *ArrayGV =
      new llvm::GlobalVariable(*M, AT, /*IsConstant*/ isConst, linkage,
                               /*InitVal*/ InitVal, GV->getName() + ".v",
                               /*InsertBefore*/ nullptr, TLMode, AddressSpace);
  // Add debug info.
  if (m_HasDbgInfo) {
    DebugInfoFinder &Finder = HLM->GetOrCreateDebugInfoFinder();
    HLModule::UpdateGlobalVariableDebugInfo(GV, Finder, ArrayGV);
  }

  // Replace GV with ArrayGV.
  if (GV->getType()->getPointerElementType()->isVectorTy()) {
    VectorToArray(GV, ArrayGV);
  } else {
    VectorArrayToArray(GV, ArrayGV);
  }
  GV->removeDeadConstantUsers();
  GV->eraseFromParent();
}

bool DynamicIndexingVectorToArray::runOnModule(Module &M) {
  if (!M.HasHLModule())
    return false;
  HLModule *HLM = &M.GetHLModule();

  // Load up debug information, to cross-reference values and the instructions
  // used to load them.
  m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    runOnFunction(F);
  }

  // Work on internal global.
  std::vector<GlobalVariable *> vecGVs;
  for (GlobalVariable &GV : M.globals()) {
    if (HLModule::IsStaticGlobal(&GV) || HLModule::IsSharedMemoryGlobal(&GV)) {
      Type *Ty = GV.getType()->getPointerElementType();
      if (ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
        while (isa<ArrayType>(Ty)) {
          Ty = Ty->getArrayElementType();
        }
      }
      bool isVecTy = isa<VectorType>(Ty);

      if (isVecTy && !GV.user_empty())
        vecGVs.emplace_back(&GV);
    }
  }

  for (GlobalVariable *GV : vecGVs) {
    runOnInternalGlobal(GV, HLM);
  }

  // Merge GEP for all interal globals.
  for (GlobalVariable &GV : M.globals()) {
    if (HLModule::IsStaticGlobal(&GV) || HLModule::IsSharedMemoryGlobal(&GV)) {
      // Merge all GEP.
      MergeGepUse(&GV);
    }
  }
  return true;
}

char DynamicIndexingVectorToArray::ID = 0;

INITIALIZE_PASS(DynamicIndexingVectorToArray, "dynamic-vector-to-array",
  "Replace dynamic indexing vector with array", false,
  false)

// Public interface to the SROA_Parameter_HLSL pass
ModulePass *llvm::createDynamicIndexingVectorToArrayPass(bool ReplaceAllVector) {
  return new DynamicIndexingVectorToArray(ReplaceAllVector);
}

//===----------------------------------------------------------------------===//
// Flatten multi dim array into 1 dim.
//===----------------------------------------------------------------------===//

namespace {
class MultiDimArrayToOneDimArray : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit MultiDimArrayToOneDimArray() : ModulePass(ID) {}
  const char *getPassName() const override { return "Flatten multi-dim array into one-dim array"; }

  bool runOnModule(Module & M) override;
private:
  void flattenMultiDimArray(Value *MultiDim, Value *OneDim);
  void flattenAlloca(AllocaInst *AI);
  void flattenGlobal(GlobalVariable *GV, DxilModule *DM);

  bool m_HasDbgInfo;
  Instruction *m_GlobalInsertPoint;
};

bool IsMultiDimArrayType(Type *Ty) {
  ArrayType *AT = dyn_cast<ArrayType>(Ty);
  if (AT)
    return isa<ArrayType>(AT->getElementType());
  return false;
}

}

void MultiDimArrayToOneDimArray::flattenMultiDimArray(Value *MultiDim, Value *OneDim) {
  // All users should be element type.
  // Replace users of AI.
  for (auto it = MultiDim->user_begin(); it != MultiDim->user_end();) {
    User *U = *(it++);
    if (U->user_empty())
      continue;
    // Must be GEP.
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U);

    Instruction *InsertPoint = GEP;
    if (!InsertPoint) {
      DXASSERT_NOMSG(isa<GEPOperator>(U));
      // NewGEP must be GEPOperator too.
      // No instruction will be build.
      InsertPoint = m_GlobalInsertPoint;
    }
    IRBuilder<> Builder(InsertPoint);
    gep_type_iterator GEPIt = gep_type_begin(U), E = gep_type_end(U);

    Value *PtrOffset = GEPIt.getOperand();
    ++GEPIt;
    Value *ArrayIdx = GEPIt.getOperand();
    ++GEPIt;
    Value *VecIdx = nullptr;
    for (; GEPIt != E; ++GEPIt) {
      if (GEPIt->isArrayTy()) {
        unsigned arraySize = GEPIt->getArrayNumElements();
        Value *V = GEPIt.getOperand();
        ArrayIdx = Builder.CreateMul(ArrayIdx, Builder.getInt32(arraySize));
        ArrayIdx = Builder.CreateAdd(V, ArrayIdx);
      } else {
        DXASSERT_NOMSG(isa<VectorType>(*GEPIt));
        VecIdx = GEPIt.getOperand();
      }
    }
    Value *NewGEP = nullptr;
    if (!VecIdx)
      NewGEP = Builder.CreateGEP(OneDim, {PtrOffset, ArrayIdx});
    else
      NewGEP = Builder.CreateGEP(OneDim, {PtrOffset, ArrayIdx, VecIdx});

    U->replaceAllUsesWith(NewGEP);
    if (GEP)
      GEP->eraseFromParent();
  }
}

void MultiDimArrayToOneDimArray::flattenAlloca(AllocaInst *AI) {
  Type *Ty = AI->getAllocatedType();

  ArrayType *AT = cast<ArrayType>(Ty);
  unsigned arraySize = AT->getNumElements();

  Type *EltTy = AT->getElementType();
  // support multi level of array
  while (EltTy->isArrayTy()) {
    ArrayType *ElAT = cast<ArrayType>(EltTy);
    arraySize *= ElAT->getNumElements();
    EltTy = ElAT->getElementType();
  }

  AT = ArrayType::get(EltTy, arraySize);
  IRBuilder<> Builder(AI);
  Value *NewAI = Builder.CreateAlloca(AT);
  // Merge all GEP of AI.
  MergeGepUse(AI);

  flattenMultiDimArray(AI, NewAI);
  AI->eraseFromParent();
}

void MultiDimArrayToOneDimArray::flattenGlobal(GlobalVariable *GV, DxilModule *DM) {
  Type *Ty = GV->getType()->getElementType();

  ArrayType *AT = cast<ArrayType>(Ty);
  unsigned arraySize = AT->getNumElements();

  Type *EltTy = AT->getElementType();
  // support multi level of array
  while (EltTy->isArrayTy()) {
    ArrayType *ElAT = cast<ArrayType>(EltTy);
    arraySize *= ElAT->getNumElements();
    EltTy = ElAT->getElementType();
  }

  AT = ArrayType::get(EltTy, arraySize);
  Constant *InitVal = GV->getInitializer();
  if (InitVal) {
    // MultiDim array init should be done by store.
    if (isa<ConstantAggregateZero>(InitVal))
      InitVal = ConstantAggregateZero::get(AT);
    else if (isa<UndefValue>(InitVal))
      InitVal = UndefValue::get(AT);
    else
      DXASSERT(0, "invalid initializer");
  } else {
    InitVal = UndefValue::get(AT);
  }
  GlobalVariable *NewGV = new GlobalVariable(
      *GV->getParent(), AT, /*IsConstant*/ GV->isConstant(), GV->getLinkage(),
      /*InitVal*/ InitVal, GV->getName() + ".1dim", /*InsertBefore*/ GV,
      GV->getThreadLocalMode(), GV->getType()->getAddressSpace());

  // Update debuginfo.
  if (m_HasDbgInfo) {
    llvm::DebugInfoFinder &Finder = DM->GetOrCreateDebugInfoFinder();
    HLModule::UpdateGlobalVariableDebugInfo(GV, Finder, NewGV);
  }

  flattenMultiDimArray(GV, NewGV);
  GV->removeDeadConstantUsers();
  GV->eraseFromParent();
}

static void CheckInBoundForTGSM(GlobalVariable &GV, const DataLayout &DL) {
  for (User * U : GV.users()) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
      bool allImmIndex = true;
      for (auto Idx = GEP->idx_begin(), E=GEP->idx_end(); Idx != E; Idx++) {
        if (!isa<ConstantInt>(Idx)) {
          allImmIndex = false;
          break;
        }
      }
      if (!allImmIndex)
        GEP->setIsInBounds(false);
      else {
        Value *Ptr = GEP->getPointerOperand();
        unsigned size = DL.getTypeAllocSize(Ptr->getType()->getPointerElementType());
        unsigned valSize = DL.getTypeAllocSize(GEP->getType()->getPointerElementType());
        SmallVector<Value *, 8> Indices(GEP->idx_begin(), GEP->idx_end());
        unsigned offset =
            DL.getIndexedOffset(GEP->getPointerOperandType(), Indices);
        if ((offset+valSize) > size)
          GEP->setIsInBounds(false);
      }
    }
  }
}

bool MultiDimArrayToOneDimArray::runOnModule(Module &M) {
  if (!M.HasDxilModule())
    return false;
  DxilModule *DM = &M.GetDxilModule();

  m_GlobalInsertPoint =
      DM->GetEntryFunction()->getEntryBlock().getFirstInsertionPt();
  // Load up debug information, to cross-reference values and the instructions
  // used to load them.
  m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    BasicBlock &BB = F.getEntryBlock();
    for (BasicBlock::iterator I = BB.begin(); I != BB.end();) {
      if (AllocaInst *A = dyn_cast<AllocaInst>(I++)) {
        if (IsMultiDimArrayType(A->getAllocatedType()))
          flattenAlloca(A);
      }
    }
  }

  // Flatten internal global.
  std::vector<GlobalVariable *> multiDimGVs;
  for (GlobalVariable &GV : M.globals()) {
    if (HLModule::IsStaticGlobal(&GV) || HLModule::IsSharedMemoryGlobal(&GV)) {
      // Merge all GEP.
      MergeGepUse(&GV);
      if (IsMultiDimArrayType(GV.getType()->getElementType()) &&
          !GV.user_empty())
        multiDimGVs.emplace_back(&GV);
    }
  }

  for (GlobalVariable *GV : multiDimGVs)
    flattenGlobal(GV, DM);

  const DataLayout &DL = M.getDataLayout();
  // Clear inbound for GEP which has none-const index.
  for (GlobalVariable &GV : M.globals()) {
    if (HLModule::IsSharedMemoryGlobal(&GV)) {
      CheckInBoundForTGSM(GV, DL);
    }
  }

  return true;
}

char MultiDimArrayToOneDimArray::ID = 0;

INITIALIZE_PASS(MultiDimArrayToOneDimArray, "multi-dim-one-dim",
  "Flatten multi-dim array into one-dim array", false,
  false)

// Public interface to the SROA_Parameter_HLSL pass
ModulePass *llvm::createMultiDimArrayToOneDimArrayPass() {
  return new MultiDimArrayToOneDimArray();
}