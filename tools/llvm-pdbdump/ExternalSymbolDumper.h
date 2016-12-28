//===- ExternalSymbolDumper.h --------------------------------- *- C++ --*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ExternalSymbolDumper.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_EXTERNALSYMBOLDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_EXTERNALSYMBOLDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"

namespace llvm {

class LinePrinter;

class ExternalSymbolDumper : public PDBSymDumper {
public:
  ExternalSymbolDumper(LinePrinter &P);

  void start(const PDBSymbolExe &Symbol);

  void dump(const PDBSymbolPublicSymbol &Symbol) override;

private:
  LinePrinter &Printer;
};
}

#endif
