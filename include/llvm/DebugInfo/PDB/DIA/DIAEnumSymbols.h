//==- DIAEnumSymbols.h - DIA Symbol Enumerator impl --------------*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIAEnumSymbols.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIAENUMSYMBOLS_H
#define LLVM_DEBUGINFO_PDB_DIA_DIAENUMSYMBOLS_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBEnumChildren.h"

namespace llvm {

class DIASession;

class DIAEnumSymbols : public IPDBEnumChildren<PDBSymbol> {
public:
  explicit DIAEnumSymbols(const DIASession &Session,
                          CComPtr<IDiaEnumSymbols> DiaEnumerator);

  uint32_t getChildCount() const override;
  std::unique_ptr<PDBSymbol> getChildAtIndex(uint32_t Index) const override;
  std::unique_ptr<PDBSymbol> getNext() override;
  void reset() override;
  DIAEnumSymbols *clone() const override;

private:
  const DIASession &Session;
  CComPtr<IDiaEnumSymbols> Enumerator;
};
}

#endif
