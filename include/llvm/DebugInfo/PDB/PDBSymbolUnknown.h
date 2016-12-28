//===- PDBSymbolUnknown.h - unknown symbol type -----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PDBSymbolUnknown.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_PDBSYMBOLUNKNOWN_H
#define LLVM_DEBUGINFO_PDB_PDBSYMBOLUNKNOWN_H

#include "PDBSymbol.h"

namespace llvm {

class raw_ostream;

class PDBSymbolUnknown : public PDBSymbol {
public:
  PDBSymbolUnknown(const IPDBSession &PDBSession,
                   std::unique_ptr<IPDBRawSymbol> UnknownSymbol);

  void dump(PDBSymDumper &Dumper) const override;

  static bool classof(const PDBSymbol *S) {
    return (S->getSymTag() == PDB_SymType::None ||
            S->getSymTag() >= PDB_SymType::Max);
  }
};

} // namespace llvm

#endif // LLVM_DEBUGINFO_PDB_PDBSYMBOLUNKNOWN_H
