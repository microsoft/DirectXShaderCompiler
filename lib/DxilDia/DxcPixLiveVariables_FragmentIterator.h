///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcPixLiveVariables_FragmentIterator.h                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Declares the FragmentIterator API. This API is used to traverse           //
// DIVariables and assign alloca registers to DIBasicTypes.                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

namespace llvm
{
class AllocaInst;
class DataLayout;
class DbgDeclareInst;
class DIExpression;
}  // namespace llvm

namespace dxil_debug_info
{
class FragmentIterator
{
public:
  virtual ~FragmentIterator() = default;

  unsigned FragmentSizeInBits() const;

  virtual unsigned CurrOffsetInBits() = 0;

  bool Next(unsigned *FragmentIndex);

  static std::unique_ptr<FragmentIterator> Create
  (
      llvm::DbgDeclareInst *DbgDeclare,
      const llvm::DataLayout &DataLayout,
      llvm::AllocaInst *Alloca,
      llvm::DIExpression *Expression
  );

protected:
  FragmentIterator(
      unsigned NumFragments,
      unsigned FragmentSizeInBits,
      unsigned InitialOffsetInBits);

  unsigned m_CurrFragment = 0;
  unsigned m_NumFragments = 0;
  unsigned m_FragmentSizeInBits = 0;
  unsigned m_InitialOffsetInBits = 0;
};
}  // namespace dxil_debug_info
