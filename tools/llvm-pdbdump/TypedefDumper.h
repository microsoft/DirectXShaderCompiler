//===- TypedefDumper.h - llvm-pdbdump typedef dumper ---------*- C++ ----*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TypedefDumper.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_TYPEDEFDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_TYPEDEFDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"

namespace llvm {

class LinePrinter;

class TypedefDumper : public PDBSymDumper {
public:
  TypedefDumper(LinePrinter &P);

  void start(const PDBSymbolTypeTypedef &Symbol);

  void dump(const PDBSymbolTypeArray &Symbol) override;
  void dump(const PDBSymbolTypeBuiltin &Symbol) override;
  void dump(const PDBSymbolTypeEnum &Symbol) override;
  void dump(const PDBSymbolTypeFunctionSig &Symbol) override;
  void dump(const PDBSymbolTypePointer &Symbol) override;
  void dump(const PDBSymbolTypeUDT &Symbol) override;

private:
  LinePrinter &Printer;
};
}

#endif
