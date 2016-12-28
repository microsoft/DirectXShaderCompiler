//===- TypeDumper.h - PDBSymDumper implementation for types *- C++ ------*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TypeDumper.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_TYPEDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_TYPEDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"

namespace llvm {

class LinePrinter;

class TypeDumper : public PDBSymDumper {
public:
  TypeDumper(LinePrinter &P);

  void start(const PDBSymbolExe &Exe);

  void dump(const PDBSymbolTypeEnum &Symbol) override;
  void dump(const PDBSymbolTypeTypedef &Symbol) override;
  void dump(const PDBSymbolTypeUDT &Symbol) override;

private:
  LinePrinter &Printer;
};
}

#endif
