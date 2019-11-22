///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcPixLiveVariables_FragmentIterator.cpp                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the FragmentIterator.                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "DxcPixLiveVariables_FragmentIterator.h"

#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/Support/exception.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include <vector>

namespace dxil_debug_info
{
class DILayoutFragmentIterator : public FragmentIterator
{
public:
  DILayoutFragmentIterator(
      const llvm::DataLayout &DataLayout,
      llvm::AllocaInst *Alloca,
      llvm::DIExpression *Expression);

  unsigned CurrOffsetInBits() override;
};

class DebugLayoutFragmentIterator : public FragmentIterator
{
public:
  DebugLayoutFragmentIterator(
      const llvm::DataLayout& DataLayout,
      llvm::AllocaInst* Alloca,
      unsigned InitialOffsetInBits,
      const std::vector<hlsl::DxilDIArrayDim> &ArrayDims);

  unsigned CurrOffsetInBits() override;

private:
  std::vector<hlsl::DxilDIArrayDim> m_ArrayDims;
};
}  // namespace dxil_debug_info

dxil_debug_info::FragmentIterator::FragmentIterator(
    unsigned NumFragments,
    unsigned FragmentSizeInBits,
    unsigned InitialOffsetInBits
) : m_NumFragments(NumFragments)
  , m_FragmentSizeInBits(FragmentSizeInBits)
  , m_InitialOffsetInBits(InitialOffsetInBits)
{
}

unsigned dxil_debug_info::FragmentIterator::FragmentSizeInBits() const
{
  return m_FragmentSizeInBits;
}

bool dxil_debug_info::FragmentIterator::Next(
    unsigned* FragmentIndex
)
{
  if (m_CurrFragment >= m_NumFragments)
  {
    return false;
  }

  *FragmentIndex = m_CurrFragment++;
  return true;
}

static unsigned NumAllocaElements(llvm::AllocaInst *Alloca)
{
  llvm::Type* FragmentTy = Alloca->getAllocatedType();
  if (auto* ArrayTy = llvm::dyn_cast<llvm::ArrayType>(FragmentTy))
  {
    return ArrayTy->getNumElements();
  }

  const unsigned NumElements = 1;
  return NumElements;
}

static unsigned FragmentSizeInBitsFromAlloca(
    const llvm::DataLayout &DataLayout,
    llvm::AllocaInst *Alloca
)
{
  llvm::Type *FragmentTy = Alloca->getAllocatedType();
  if (auto *ArrayTy = llvm::dyn_cast<llvm::ArrayType>(FragmentTy))
  {
    FragmentTy = ArrayTy->getElementType();
  }

  const unsigned FragmentSizeInBits =
      DataLayout.getTypeAllocSizeInBits(FragmentTy);

  return FragmentSizeInBits;
}

static unsigned InitialOffsetInBitsFromDIExpression(
    const llvm::DataLayout &DataLayout,
    llvm::AllocaInst *Alloca,
    llvm::DIExpression *Expression
)
{
  unsigned FragmentOffsetInBits = 0;
  if (Expression->getNumElements() > 0)
  {
    if (!Expression->isBitPiece())
    {
      assert(!"Unhandled DIExpression");
      throw hlsl::Exception(E_FAIL, "Unhandled DIExpression");
    }

    FragmentOffsetInBits = Expression->getBitPieceOffset();
    assert(Expression->getBitPieceSize() ==
           DataLayout.getTypeAllocSizeInBits(
              Alloca->getAllocatedType()));
  }

  return FragmentOffsetInBits;
}

dxil_debug_info::DILayoutFragmentIterator::DILayoutFragmentIterator(
    const llvm::DataLayout& DataLayout,
    llvm::AllocaInst* Alloca,
    llvm::DIExpression* Expression)
  : FragmentIterator(NumAllocaElements(Alloca),
                     FragmentSizeInBitsFromAlloca(DataLayout, Alloca),
                     InitialOffsetInBitsFromDIExpression(DataLayout,
                                                         Alloca,
                                                         Expression))
{
}

unsigned dxil_debug_info::DILayoutFragmentIterator::CurrOffsetInBits()
{
  return
      m_InitialOffsetInBits + (m_CurrFragment - 1) * m_FragmentSizeInBits;
}

static unsigned NumFragmentsFromArrayDims(
    const std::vector<hlsl::DxilDIArrayDim>& ArrayDims
)
{
  unsigned TotalNumFragments = 1;
  for (const hlsl::DxilDIArrayDim& ArrayDim : ArrayDims) {
    TotalNumFragments *= ArrayDim.NumElements;
  }
  return TotalNumFragments;
}

static unsigned FragmentSizeInBitsFrom(
    const llvm::DataLayout& DataLayout,
    llvm::AllocaInst *Alloca,
    unsigned TotalNumFragments
)
{
  const unsigned TotalSizeInBits =
      DataLayout.getTypeAllocSizeInBits(
          Alloca->getAllocatedType());

  if (TotalNumFragments == 0 || TotalSizeInBits % TotalNumFragments != 0) {
    assert(!"Malformed variable debug layout metadata.");
    throw hlsl::Exception(
        E_FAIL,
        "Malformed variable debug layout metadata.");
  }

  const unsigned FragmentSizeInBits = TotalSizeInBits / TotalNumFragments;
  return FragmentSizeInBits;
}

dxil_debug_info::DebugLayoutFragmentIterator::DebugLayoutFragmentIterator(
    const llvm::DataLayout& DataLayout,
    llvm::AllocaInst* Alloca,
    unsigned InitialOffsetInBits,
    const std::vector<hlsl::DxilDIArrayDim>& ArrayDims)
  : FragmentIterator(NumFragmentsFromArrayDims(ArrayDims),
                     FragmentSizeInBitsFrom(DataLayout,
                                            Alloca, 
                                            NumFragmentsFromArrayDims(ArrayDims)),
                     InitialOffsetInBits)
  , m_ArrayDims(ArrayDims)
{
}

unsigned dxil_debug_info::DebugLayoutFragmentIterator::CurrOffsetInBits()
{
  // Figure out the offset of this fragment in the original
  unsigned FragmentOffsetInBits = m_InitialOffsetInBits;
  unsigned RemainingIndex = m_CurrFragment - 1;
  for (const hlsl::DxilDIArrayDim& ArrayDim : m_ArrayDims) {
      FragmentOffsetInBits += (RemainingIndex % ArrayDim.NumElements) * ArrayDim.StrideInBits;
      RemainingIndex /= ArrayDim.NumElements;
  }
  assert(RemainingIndex == 0);
  return FragmentOffsetInBits;
}


std::unique_ptr<dxil_debug_info::FragmentIterator>
dxil_debug_info::FragmentIterator::Create
(
    llvm::DbgDeclareInst *DbgDeclare,
    const llvm::DataLayout& DataLayout,
    llvm::AllocaInst* Alloca,
    llvm::DIExpression* Expression
)
{
  bool HasVariableDebugLayout = false;
  unsigned FirstFragmentOffsetInBits;
  std::vector<hlsl::DxilDIArrayDim> ArrayDims;

  std::unique_ptr<dxil_debug_info::FragmentIterator> Iter;

  try
  {
    HasVariableDebugLayout = 
        hlsl::DxilMDHelper::GetVariableDebugLayout(
            DbgDeclare,
            FirstFragmentOffsetInBits,
            ArrayDims);

    if (HasVariableDebugLayout)
    {
      Iter.reset(new DebugLayoutFragmentIterator(
          DataLayout,
          Alloca,
          FirstFragmentOffsetInBits,
          ArrayDims));
    }
    else
    {
      Iter.reset(new DILayoutFragmentIterator(
          DataLayout,
          Alloca,
          Expression));
    }
  }
  catch (const hlsl::Exception &)
  {
    return nullptr;
  }

  return Iter;
}
