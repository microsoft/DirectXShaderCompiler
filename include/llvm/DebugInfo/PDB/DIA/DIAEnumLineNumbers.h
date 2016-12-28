//==- DIAEnumLineNumbers.h - DIA Line Number Enumerator impl -----*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIAEnumLineNumbers.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIAENUMLINENUMBERS_H
#define LLVM_DEBUGINFO_PDB_DIA_DIAENUMLINENUMBERS_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBEnumChildren.h"

namespace llvm {

class IPDBLineNumber;

class DIAEnumLineNumbers : public IPDBEnumChildren<IPDBLineNumber> {
public:
  explicit DIAEnumLineNumbers(CComPtr<IDiaEnumLineNumbers> DiaEnumerator);

  uint32_t getChildCount() const override;
  ChildTypePtr getChildAtIndex(uint32_t Index) const override;
  ChildTypePtr getNext() override;
  void reset() override;
  DIAEnumLineNumbers *clone() const override;

private:
  CComPtr<IDiaEnumLineNumbers> Enumerator;
};
}

#endif
