//===- VariableDumper.h - PDBSymDumper implementation for types -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// VariableDumper.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_VARIABLEDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_VARIABLEDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

class LinePrinter;

class VariableDumper : public PDBSymDumper {
public:
  VariableDumper(LinePrinter &P);

  void start(const PDBSymbolData &Var);

  void dump(const PDBSymbolTypeBuiltin &Symbol) override;
  void dump(const PDBSymbolTypeEnum &Symbol) override;
  void dump(const PDBSymbolTypeFunctionSig &Symbol) override;
  void dump(const PDBSymbolTypePointer &Symbol) override;
  void dump(const PDBSymbolTypeTypedef &Symbol) override;
  void dump(const PDBSymbolTypeUDT &Symbol) override;

private:
  void dumpSymbolTypeAndName(const PDBSymbol &Type, StringRef Name);
  bool tryDumpFunctionPointer(const PDBSymbol &Type, StringRef Name);

  LinePrinter &Printer;
};
}

#endif
