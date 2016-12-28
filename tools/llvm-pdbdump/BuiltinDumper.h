//===- BuiltinDumper.h ---------------------------------------- *- C++ --*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// BuiltinDumper.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TOOLS_LLVMPDBDUMP_BUILTINDUMPER_H
#define LLVM_TOOLS_LLVMPDBDUMP_BUILTINDUMPER_H

#include "llvm/DebugInfo/PDB/PDBSymDumper.h"

namespace llvm {

class LinePrinter;

class BuiltinDumper : public PDBSymDumper {
public:
  BuiltinDumper(LinePrinter &P);

  void start(const PDBSymbolTypeBuiltin &Symbol);

private:
  LinePrinter &Printer;
};
}

#endif
