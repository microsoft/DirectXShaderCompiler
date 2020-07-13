///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilDbgValueToDbgDeclare.cpp                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Converts calls to llvm.dbg.value to llvm.dbg.declare + alloca + stores.   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <memory>
#include <map>
#include <unordered_map>
#include <utility>

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "dxil-dbg-value-to-dbg-declare"

namespace {
using OffsetInBits = unsigned;
using SizeInBits = unsigned;

// OffsetManager is used to map between "packed" and aligned offsets.
//
// For example, the aligned offsets for a struct [float, half, int, double]
// will be {0, 32, 64, 128} (assuming 32 bit alignments for ints, and 64
// bit for doubles), while the packed offsets will be {0, 32, 48, 80}.
//
// This mapping makes it easier to deal with llvm.dbg.values whose value
// operand does not match exactly the Variable operand's type.
class OffsetManager
{
public:
  OffsetManager() = default;

  // AlignTo aligns the current aligned offset to Ty's natural alignment.
  void AlignTo(
      llvm::DIType *Ty
  )
  {
    // This is some magic arithmetic. Here's an example:
    //
    // Assume the natural alignment for Ty is 16 bits. Then
    //
    //     AlignMask = 0x0000000f(15)
    //
    // If the current aligned offset is 
    //
    //     CurrentAlignedOffset = 0x00000048(72)
    //
    // Then
    //
    //     T = CurrentAlignOffset + AlignMask = 0x00000057(87)
    //
    // Which mean
    //
    //     T & ~CurrentOffset = 0x00000050(80)
    //
    // is the aligned offset where Ty should be placed.
    const unsigned AlignMask = Ty->getAlignInBits() - 1;
    m_CurrentAlignedOffset =
        (m_CurrentAlignedOffset + AlignMask) & ~AlignMask;
  }

  // Add is used to "add" an aggregate element (struct field, array element)
  // at the current aligned/packed offsets, bumping them by Ty's size.
  OffsetInBits Add(
      llvm::DIBasicType *Ty
  )
  {
    m_PackedOffsetToAlignedOffset[m_CurrentPackedOffset] = m_CurrentAlignedOffset;
    m_AlignedOffsetToPackedOffset[m_CurrentAlignedOffset] = m_CurrentPackedOffset;

    const OffsetInBits Ret = m_CurrentAlignedOffset;
    m_CurrentPackedOffset += Ty->getSizeInBits();
    m_CurrentAlignedOffset += Ty->getSizeInBits();

    return Ret;
  }

  // AlignToAndAddUnhandledType is used for error handling when Ty
  // could not be handled by the transformation. This is a best-effort
  // way to continue the pass by ignoring the current type and hoping
  // that adding Ty as a blob other fields/elements added will land
  // in the proper offset.
  void AlignToAndAddUnhandledType(
      llvm::DIType *Ty
  )
  {
    AlignTo(Ty);
    m_CurrentPackedOffset += Ty->getSizeInBits();
    m_CurrentAlignedOffset += Ty->getSizeInBits();
  }

  bool GetAlignedOffsetFromPackedOffset(
      OffsetInBits PackedOffset,
      OffsetInBits *AlignedOffset
  ) const
  {
    return GetOffsetWithMap(
        m_PackedOffsetToAlignedOffset,
        PackedOffset,
        AlignedOffset);
  }

  OffsetInBits GetPackedOffsetFromAlignedOffset(
      OffsetInBits AlignedOffset,
      OffsetInBits *PackedOffset
  ) const
  {
    return GetOffsetWithMap(
        m_PackedOffsetToAlignedOffset,
        AlignedOffset,
        PackedOffset);
  }

  OffsetInBits GetCurrentPackedOffset() const
  {
    return m_CurrentPackedOffset;
  }

  OffsetInBits GetCurrentAlignedOffset() const
  {
      return m_CurrentAlignedOffset;
  }

private:
  OffsetInBits m_CurrentPackedOffset = 0;
  OffsetInBits m_CurrentAlignedOffset = 0;

  using OffsetMap = std::unordered_map<OffsetInBits, OffsetInBits>;

  OffsetMap m_PackedOffsetToAlignedOffset;
  OffsetMap m_AlignedOffsetToPackedOffset;

  static bool GetOffsetWithMap(
      const OffsetMap &Map,
      OffsetInBits SrcOffset,
      OffsetInBits *DstOffset
  )
  {
    auto it = Map.find(SrcOffset);
    if (it == Map.end())
    {
      return false;
    }

    *DstOffset = it->second;
    return true;
  }
};

// VariableRegisters contains the logic for traversing a DIType T and
// creating AllocaInsts that map back to a specific offset within T.
class VariableRegisters
{
public:
  VariableRegisters(
      llvm::DIVariable *Variable,
      llvm::Module *M
  );

  llvm::AllocaInst *GetRegisterForAlignedOffset(
      OffsetInBits AlignedOffset
  ) const;

  const OffsetManager& GetOffsetManager() const
  {
    return m_Offsets;
  }

private:
  void PopulateAllocaMap(
      llvm::DIType *Ty
  );

  void PopulateAllocaMap_BasicType(
      llvm::DIBasicType *Ty
  );

  void PopulateAllocaMap_ArrayType(
      llvm::DICompositeType *Ty
  );

  void PopulateAllocaMap_StructType(
      llvm::DICompositeType* Ty
  );

  llvm::DILocation *GetVariableLocation() const;
  llvm::Value *GetMetadataAsValue(
      llvm::Metadata *M
  ) const;
  llvm::DIExpression *GetDIExpression(
      llvm::DIType *Ty,
      OffsetInBits Offset
  ) const;

  llvm::DIVariable* m_Variable = nullptr;
  llvm::IRBuilder<> m_B;
  llvm::Function *m_DbgDeclareFn = nullptr;

  OffsetManager m_Offsets;
  std::unordered_map<OffsetInBits, llvm::AllocaInst *> m_AlignedOffsetToAlloca;
};

class DxilDbgValueToDbgDeclare : public llvm::ModulePass {
public:
  static char ID;
  DxilDbgValueToDbgDeclare() : llvm::ModulePass(ID) {}

  bool runOnModule(
      llvm::Module &M
  ) override;

private:
  void handleDbgValue(
      llvm::Module &M,
      llvm::DbgValueInst *DbgValue);

  std::unordered_map<llvm::DIVariable *, std::unique_ptr<VariableRegisters>> m_Registers;
};
}  // namespace

char DxilDbgValueToDbgDeclare::ID = 0;

struct ValueAndOffset
{
    llvm::Value *m_V;
    OffsetInBits m_PackedOffset;
};

// SplitValue splits an llvm::Value into possibly multiple
// scalar Values. Those scalar values will later be "stored"
// into their corresponding register.
static OffsetInBits SplitValue(
    llvm::Value *V,
    OffsetInBits CurrentOffset,
    std::vector<ValueAndOffset> *Values,
    llvm::IRBuilder<>& B
)
{
  auto *VTy = V->getType();
  if (auto *ArrTy = llvm::dyn_cast<llvm::ArrayType>(VTy))
  {
    for (unsigned i = 0; i < ArrTy->getNumElements(); ++i)
    {
      CurrentOffset = SplitValue(
          B.CreateExtractValue(V, {i}),
          CurrentOffset,
          Values,
          B);
    }
  }
  else if (auto *StTy = llvm::dyn_cast<llvm::StructType>(VTy))
  {
    for (unsigned i = 0; i < StTy->getNumElements(); ++i)
    {
      CurrentOffset = SplitValue(
          B.CreateExtractValue(V, {i}),
          CurrentOffset,
          Values,
          B);
    }
  }
  else if (auto *VecTy = llvm::dyn_cast<llvm::VectorType>(VTy))
  {
    for (unsigned i = 0; i < VecTy->getNumElements(); ++i)
    {
      CurrentOffset = SplitValue(
          B.CreateExtractElement(V, i),
          CurrentOffset,
          Values,
          B);
    }
  }
  else
  {
    assert(VTy->isFloatTy() || VTy->isDoubleTy() || VTy->isHalfTy() ||
           VTy->isIntegerTy(32) || VTy->isIntegerTy(64) || VTy->isIntegerTy(16));
    Values->emplace_back(ValueAndOffset{V, CurrentOffset});
    CurrentOffset += VTy->getScalarSizeInBits();
  }

  return CurrentOffset;
}

// A more convenient version of SplitValue.
static std::vector<ValueAndOffset> SplitValue(
    llvm::Value* V,
    OffsetInBits CurrentOffset,
    llvm::IRBuilder<>& B
)
{
    std::vector<ValueAndOffset> Ret;
    SplitValue(V, CurrentOffset, &Ret, B);
    return Ret;
}

// Convenient helper for parsing a DIExpression's offset.
static OffsetInBits GetAlignedOffsetFromDIExpression(
    llvm::DIExpression *Exp
)
{
  if (!Exp->isBitPiece())
  {
    return 0;
  }

  return Exp->getBitPieceOffset();
}

bool DxilDbgValueToDbgDeclare::runOnModule(
    llvm::Module &M
)
{
  auto *DbgValueFn =
      llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::dbg_value);

  bool Changed = false;
  for (auto it = DbgValueFn->user_begin(); it != DbgValueFn->user_end(); )
  {
    llvm::User *User = *it++;

    if (auto *DbgValue = llvm::dyn_cast<llvm::DbgValueInst>(User))
    {
      Changed = true;
      handleDbgValue(M, DbgValue);
      DbgValue->eraseFromParent();
    }
  }
  return Changed;
}

void DxilDbgValueToDbgDeclare::handleDbgValue(
    llvm::Module& M,
    llvm::DbgValueInst* DbgValue)
{
  llvm::IRBuilder<> B(M.getContext());
  auto* Zero = B.getInt32(0);

  llvm::DIVariable *Variable = DbgValue->getVariable();
  auto &Register = m_Registers[DbgValue->getVariable()];
  if (Register == nullptr)
  {
    Register.reset(new VariableRegisters(Variable, &M));
  }

  llvm::Value *V = DbgValue->getValue();
  if (V == nullptr)
  {
    // The metadata contained a null Value, so we ignore it. This
    // seems to be a dxcompiler bug.
    return;
  }

  // Convert the offset from DbgValue's expression to a packed
  // offset, which we'll need in order to determine the (packed)
  // offset of each scalar Value in DbgValue.
  const OffsetInBits AlignedOffsetFromVar =
      GetAlignedOffsetFromDIExpression(DbgValue->getExpression());
  OffsetInBits PackedOffsetFromVar;
  const OffsetManager& Offsets = Register->GetOffsetManager();
  if (!Offsets.GetPackedOffsetFromAlignedOffset(AlignedOffsetFromVar,
                                                &PackedOffsetFromVar))
  {
    assert(!"Failed to find packed offset");
    return;
  }

  const OffsetInBits InitialOffset = PackedOffsetFromVar;
  B.SetInsertPoint(DbgValue);
  B.SetCurrentDebugLocation(llvm::DebugLoc());

  // Now traverse a list of pairs {Scalar Value, InitialOffset + Offset}.
  // InitialOffset is the offset from DbgValue's expression (i.e., the
  // offset from the Variable's start), and Offset is the Scalar Value's
  // packed offset from DbgValue's value. 
  for (const ValueAndOffset &VO : SplitValue(V, InitialOffset, B))
  {
    OffsetInBits AlignedOffset;
    if (!Offsets.GetAlignedOffsetFromPackedOffset(VO.m_PackedOffset,
                                                  &AlignedOffset))
    {
      continue;
    }

    auto* AllocaInst = Register->GetRegisterForAlignedOffset(AlignedOffset);
    if (AllocaInst == nullptr)
    {
      assert(!"Failed to find alloca for var[offset]");
      continue;
    }

    auto *GEP = B.CreateGEP(AllocaInst, {Zero, Zero});
    B.CreateStore(VO.m_V, GEP);
  }
}


llvm::AllocaInst *VariableRegisters::GetRegisterForAlignedOffset(
    OffsetInBits Offset
) const
{
  auto it = m_AlignedOffsetToAlloca.find(Offset);
  if (it == m_AlignedOffsetToAlloca.end())
  {
    return nullptr;
  }
  return it->second;
}

#ifndef NDEBUG
// DITypePeelTypeAlias peels const, typedef, and other alias types off of Ty,
// returning the unalised type.
static llvm::DIType *DITypePeelTypeAlias(
    llvm::DIType* Ty
)
{
  if (auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(Ty))
  {
    const llvm::DITypeIdentifierMap EmptyMap;
    switch (DerivedTy->getTag())
    {
    case llvm::dwarf::DW_TAG_restrict_type:
    case llvm::dwarf::DW_TAG_reference_type:
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_typedef:
      return DITypePeelTypeAlias(
          DerivedTy->getBaseType().resolve(EmptyMap));
    }
  }

  return Ty;
}
#endif // NDEBUG


VariableRegisters::VariableRegisters(
    llvm::DIVariable *Variable,
    llvm::Module *M
) : m_Variable(Variable)
  , m_B(M->GetOrCreateDxilModule().GetEntryFunction()->getEntryBlock().begin())
  , m_DbgDeclareFn(llvm::Intrinsic::getDeclaration(
      M, llvm::Intrinsic::dbg_declare))
{
  const llvm::DITypeIdentifierMap EmptyMap;
  llvm::DIType* Ty = m_Variable->getType().resolve(EmptyMap);

  PopulateAllocaMap(Ty);
  assert(m_Offsets.GetCurrentPackedOffset() ==
         DITypePeelTypeAlias(Ty)->getSizeInBits());
}

void VariableRegisters::PopulateAllocaMap(
    llvm::DIType *Ty
)
{
  if (auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(Ty))
  {
    const llvm::DITypeIdentifierMap EmptyMap;
    switch (DerivedTy->getTag())
    {
    default:
      assert(!"Unhandled DIDerivedType");
      m_Offsets.AlignToAndAddUnhandledType(DerivedTy);
      return;
    case llvm::dwarf::DW_TAG_restrict_type:
    case llvm::dwarf::DW_TAG_reference_type:
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_typedef:
      PopulateAllocaMap(
          DerivedTy->getBaseType().resolve(EmptyMap));
      return;
    }
  }
  else if (auto *CompositeTy = llvm::dyn_cast<llvm::DICompositeType>(Ty))
  {
    switch (CompositeTy->getTag())
    {
    default:
      assert(!"Unhandled DICompositeType");
      m_Offsets.AlignToAndAddUnhandledType(CompositeTy);
      return;
    case llvm::dwarf::DW_TAG_array_type:
      PopulateAllocaMap_ArrayType(CompositeTy);
      return;
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_class_type:
      PopulateAllocaMap_StructType(CompositeTy);
      return;
    }
  }
  else if (auto *BasicTy = llvm::dyn_cast<llvm::DIBasicType>(Ty))
  {
    PopulateAllocaMap_BasicType(BasicTy);
    return;
  }

  assert(!"Unhandled DIType");
  m_Offsets.AlignToAndAddUnhandledType(Ty);
}

static llvm::Type* GetLLVMTypeFromDIBasicType(
    llvm::IRBuilder<> &B,
    llvm::DIBasicType* Ty
)
{
  const SizeInBits Size = Ty->getSizeInBits();

  switch (Ty->getEncoding())
  {
  default:
    break;

  case llvm::dwarf::DW_ATE_boolean:
  case llvm::dwarf::DW_ATE_signed:
  case llvm::dwarf::DW_ATE_unsigned:
    switch(Size)
    {
    case 16:
      return B.getInt16Ty();
    case 32:
      return B.getInt32Ty();
    case 64:
      return B.getInt64Ty();
    }
    break;
  case llvm::dwarf::DW_ATE_float:
    switch(Size)
    {
    case 16:
      return B.getHalfTy();
    case 32:
      return B.getFloatTy();
    case 64:
      return B.getDoubleTy();
    }
    break;
  }

  return nullptr;
}

void VariableRegisters::PopulateAllocaMap_BasicType(
    llvm::DIBasicType *Ty
)
{
  llvm::Type* AllocaElementTy = GetLLVMTypeFromDIBasicType(m_B, Ty);
  assert(AllocaElementTy != nullptr);
  if (AllocaElementTy == nullptr)
  {
      return;
  }

  const OffsetInBits AlignedOffset = m_Offsets.Add(Ty);

  llvm::Type *AllocaTy = llvm::ArrayType::get(AllocaElementTy, 1);
  llvm::AllocaInst *&Alloca = m_AlignedOffsetToAlloca[AlignedOffset];
  Alloca = m_B.CreateAlloca(AllocaTy, m_B.getInt32(0));
  Alloca->setDebugLoc(llvm::DebugLoc());

  auto *Storage = GetMetadataAsValue(llvm::ValueAsMetadata::get(Alloca));
  auto *Variable = GetMetadataAsValue(m_Variable);
  auto *Expression = GetMetadataAsValue(GetDIExpression(Ty, AlignedOffset));
  auto *DbgDeclare = m_B.CreateCall(
      m_DbgDeclareFn,
      {Storage, Variable, Expression});
  DbgDeclare->setDebugLoc(GetVariableLocation());
}

static unsigned NumArrayElements(
    llvm::DICompositeType *Array
)
{
  if (Array->getElements().size() == 0)
  {
    return 0;
  }

  unsigned NumElements = 1;
  for (llvm::DINode *N : Array->getElements())
  {
    if (auto* Subrange = llvm::dyn_cast<llvm::DISubrange>(N))
    {
      NumElements *= Subrange->getCount();
    }
    else
    {
      assert(!"Unhandled array element");
      return 0;
    }
  }
  return NumElements;
}

void VariableRegisters::PopulateAllocaMap_ArrayType(
    llvm::DICompositeType* Ty
)
{
  unsigned NumElements = NumArrayElements(Ty);
  if (NumElements == 0)
  {
    m_Offsets.AlignToAndAddUnhandledType(Ty);
    return;
  }

  const SizeInBits ArraySizeInBits = Ty->getSizeInBits();
  (void)ArraySizeInBits;

  const llvm::DITypeIdentifierMap EmptyMap;
  llvm::DIType *ElementTy = Ty->getBaseType().resolve(EmptyMap);
  assert(ArraySizeInBits % NumElements == 0 &&
         " invalid DIArrayType"
         " - Size is not a multiple of NumElements");

  // After aligning the current aligned offset to ElementTy's natural
  // alignment, the current aligned offset must match Ty's offset
  // in bits.
  m_Offsets.AlignTo(ElementTy);

  for (unsigned i = 0; i < NumElements; ++i)
  {
    // This is only needed if ElementTy's size is not a multiple of
    // its natural alignment.
    m_Offsets.AlignTo(ElementTy);
    PopulateAllocaMap(ElementTy);
  }
}

// SortMembers traverses all of Ty's members and returns them sorted
// by their offset from Ty's start. Returns true if the function succeeds
// and false otherwise.
static bool SortMembers(
    llvm::DICompositeType* Ty,
    std::map<OffsetInBits, llvm::DIDerivedType*> *SortedMembers
)
{
  const llvm::DITypeIdentifierMap EmptyMap;
  for (auto *Element : Ty->getElements())
  {
    switch (Element->getTag())
    {
    case llvm::dwarf::DW_TAG_member: {
      if (auto *Member = llvm::dyn_cast<llvm::DIDerivedType>(Element))
      {
        if (Member->getSizeInBits()) {
          auto it = SortedMembers->emplace(std::make_pair(Member->getOffsetInBits(), Member));
          (void)it;
          assert(it.second &&
                 "Invalid DIStructType"
                 " - members with the same offset -- are unions possible?");
        }
        break;
      }
      // FALLTHROUGH
    }
    default:
      assert(!"Unhandled field type in DIStructType");
      return false;
    }
  }

  return true;
}

void VariableRegisters::PopulateAllocaMap_StructType(
    llvm::DICompositeType *Ty
)
{
  std::map<OffsetInBits, llvm::DIDerivedType*> SortedMembers;
  if (!SortMembers(Ty, &SortedMembers))
  {
      m_Offsets.AlignToAndAddUnhandledType(Ty);
      return;
  }

  m_Offsets.AlignTo(Ty);
  const OffsetInBits StructStart = m_Offsets.GetCurrentAlignedOffset();
  (void)StructStart;
  const llvm::DITypeIdentifierMap EmptyMap;

  for (auto OffsetAndMember : SortedMembers)
  {
    // Align the offsets to the member's type natural alignment. This
    // should always result in the current aligned offset being the
    // same as the member's offset.
    m_Offsets.AlignTo(OffsetAndMember.second);
    assert(m_Offsets.GetCurrentAlignedOffset() ==
           StructStart + OffsetAndMember.first &&
           "Offset mismatch in DIStructType");
    PopulateAllocaMap(
        OffsetAndMember.second->getBaseType().resolve(EmptyMap));
  }
}

llvm::DILocation *VariableRegisters::GetVariableLocation() const
{
  const unsigned DefaultColumn = 1;
  return llvm::DILocation::get(
      m_B.getContext(),
      m_Variable->getLine(),
      DefaultColumn,
      m_Variable->getScope());
}

llvm::Value *VariableRegisters::GetMetadataAsValue(
    llvm::Metadata *M
) const
{
  return llvm::MetadataAsValue::get(m_B.getContext(), M);
}

llvm::DIExpression *VariableRegisters::GetDIExpression(
    llvm::DIType *Ty,
    OffsetInBits Offset
) const
{
  llvm::SmallVector<uint64_t, 3> ExpElements;
  if (Offset != 0)
  {
    ExpElements.emplace_back(llvm::dwarf::DW_OP_bit_piece);
    ExpElements.emplace_back(Offset);
    ExpElements.emplace_back(Ty->getSizeInBits());
  }
  return llvm::DIExpression::get(m_B.getContext(), ExpElements);
}

using namespace llvm;

INITIALIZE_PASS(DxilDbgValueToDbgDeclare, DEBUG_TYPE,
                "Converts calls to dbg.value to dbg.declare + stores to new virtual registers",
                false, false)

ModulePass *llvm::createDxilDbgValueToDbgDeclarePass() {
  return new DxilDbgValueToDbgDeclare();
}
