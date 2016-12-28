//===- ExternalSymbolDumper.cpp -------------------------------- *- C++ *-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ExternalSymbolDumper.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "ExternalSymbolDumper.h"
#include "LinePrinter.h"

#include "llvm/DebugInfo/PDB/PDBSymbolExe.h"
#include "llvm/DebugInfo/PDB/PDBSymbolPublicSymbol.h"
#include "llvm/Support/Format.h"

using namespace llvm;

ExternalSymbolDumper::ExternalSymbolDumper(LinePrinter &P)
    : PDBSymDumper(true), Printer(P) {}

void ExternalSymbolDumper::start(const PDBSymbolExe &Symbol) {
  auto Vars = Symbol.findAllChildren<PDBSymbolPublicSymbol>();
  while (auto Var = Vars->getNext())
    Var->dump(*this);
}

void ExternalSymbolDumper::dump(const PDBSymbolPublicSymbol &Symbol) {
  std::string LinkageName = Symbol.getName();
  if (Printer.IsSymbolExcluded(LinkageName))
    return;

  Printer.NewLine();
  uint64_t Addr = Symbol.getVirtualAddress();

  Printer << "[";
  WithColor(Printer, PDB_ColorItem::Address).get() << format_hex(Addr, 10);
  Printer << "] ";
  WithColor(Printer, PDB_ColorItem::Identifier).get() << LinkageName;
}
