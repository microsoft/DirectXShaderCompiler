//===- TypedefDumper.cpp - PDBSymDumper impl for typedefs -------- * C++ *-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TypedefDumper.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "TypedefDumper.h"

#include "BuiltinDumper.h"
#include "FunctionDumper.h"
#include "LinePrinter.h"
#include "llvm-pdbdump.h"

#include "llvm/DebugInfo/PDB/IPDBSession.h"
#include "llvm/DebugInfo/PDB/PDBExtras.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeEnum.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeFunctionSig.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypePointer.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeTypedef.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeUDT.h"

using namespace llvm;

TypedefDumper::TypedefDumper(LinePrinter &P) : PDBSymDumper(true), Printer(P) {}

void TypedefDumper::start(const PDBSymbolTypeTypedef &Symbol) {
  WithColor(Printer, PDB_ColorItem::Keyword).get() << "typedef ";
  uint32_t TargetId = Symbol.getTypeId();
  if (auto TypeSymbol = Symbol.getSession().getSymbolById(TargetId))
    TypeSymbol->dump(*this);
  WithColor(Printer, PDB_ColorItem::Identifier).get() << " "
                                                      << Symbol.getName();
}

void TypedefDumper::dump(const PDBSymbolTypeArray &Symbol) {}

void TypedefDumper::dump(const PDBSymbolTypeBuiltin &Symbol) {
  BuiltinDumper Dumper(Printer);
  Dumper.start(Symbol);
}

void TypedefDumper::dump(const PDBSymbolTypeEnum &Symbol) {
  WithColor(Printer, PDB_ColorItem::Keyword).get() << "enum ";
  WithColor(Printer, PDB_ColorItem::Type).get() << " " << Symbol.getName();
}

void TypedefDumper::dump(const PDBSymbolTypePointer &Symbol) {
  if (Symbol.isConstType())
    WithColor(Printer, PDB_ColorItem::Keyword).get() << "const ";
  if (Symbol.isVolatileType())
    WithColor(Printer, PDB_ColorItem::Keyword).get() << "volatile ";
  uint32_t PointeeId = Symbol.getTypeId();
  auto PointeeType = Symbol.getSession().getSymbolById(PointeeId);
  if (!PointeeType)
    return;
  if (auto FuncSig = dyn_cast<PDBSymbolTypeFunctionSig>(PointeeType.get())) {
    FunctionDumper::PointerType Pointer = FunctionDumper::PointerType::Pointer;
    if (Symbol.isReference())
      Pointer = FunctionDumper::PointerType::Reference;
    FunctionDumper NestedDumper(Printer);
    NestedDumper.start(*FuncSig, nullptr, Pointer);
  } else {
    PointeeType->dump(*this);
    Printer << ((Symbol.isReference()) ? "&" : "*");
  }
}

void TypedefDumper::dump(const PDBSymbolTypeFunctionSig &Symbol) {
  FunctionDumper Dumper(Printer);
  Dumper.start(Symbol, nullptr, FunctionDumper::PointerType::None);
}

void TypedefDumper::dump(const PDBSymbolTypeUDT &Symbol) {
  WithColor(Printer, PDB_ColorItem::Keyword).get() << "class ";
  WithColor(Printer, PDB_ColorItem::Type).get() << Symbol.getName();
}
