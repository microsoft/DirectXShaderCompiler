//===- EnumDumper.h - -------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// EnumDumper.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_ENUMDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_ENUMDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"

namespace llvm {

class LinePrinter;

class EnumDumper : public PDBSymDumper {
public:
  EnumDumper(LinePrinter &P);

  void start(const PDBSymbolTypeEnum &Symbol);

private:
  LinePrinter &Printer;
};
}

#endif
