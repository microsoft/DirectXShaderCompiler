//===- PDBSymbolCompilandEnv.h - compiland environment variables *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PDBSymbolCompilandEnv.h                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_PDBSYMBOLCOMPILANDENV_H
#define LLVM_DEBUGINFO_PDB_PDBSYMBOLCOMPILANDENV_H

#include "PDBSymbol.h"
#include "PDBTypes.h"

namespace llvm {

class raw_ostream;

class PDBSymbolCompilandEnv : public PDBSymbol {
public:
  PDBSymbolCompilandEnv(const IPDBSession &PDBSession,
                        std::unique_ptr<IPDBRawSymbol> Symbol);

  DECLARE_PDB_SYMBOL_CONCRETE_TYPE(PDB_SymType::CompilandEnv)

  void dump(PDBSymDumper &Dumper) const override;

  FORWARD_SYMBOL_METHOD(getLexicalParentId)
  FORWARD_SYMBOL_METHOD(getName)
  FORWARD_SYMBOL_METHOD(getSymIndexId)
  std::string getValue() const;
};

} // namespace llvm

#endif // LLVM_DEBUGINFO_PDB_PDBSYMBOLCOMPILANDENV_H
