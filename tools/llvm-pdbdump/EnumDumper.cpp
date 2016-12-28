//===- EnumDumper.cpp -------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// EnumDumper.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "EnumDumper.h"

#include "BuiltinDumper.h"
#include "LinePrinter.h"
#include "llvm-pdbdump.h"

#include "llvm/DebugInfo/PDB/PDBSymbolData.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeBuiltin.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeEnum.h"

using namespace llvm;

EnumDumper::EnumDumper(LinePrinter &P) : PDBSymDumper(true), Printer(P) {}

void EnumDumper::start(const PDBSymbolTypeEnum &Symbol) {
  WithColor(Printer, PDB_ColorItem::Keyword).get() << "enum ";
  WithColor(Printer, PDB_ColorItem::Type).get() << Symbol.getName();
  if (!opts::NoEnumDefs) {
    auto BuiltinType = Symbol.getUnderlyingType();
    if (BuiltinType->getBuiltinType() != PDB_BuiltinType::Int ||
        BuiltinType->getLength() != 4) {
      Printer << " : ";
      BuiltinDumper Dumper(Printer);
      Dumper.start(*BuiltinType);
    }
    Printer << " {";
    Printer.Indent();
    auto EnumValues = Symbol.findAllChildren<PDBSymbolData>();
    while (auto EnumValue = EnumValues->getNext()) {
      if (EnumValue->getDataKind() != PDB_DataKind::Constant)
        continue;
      Printer.NewLine();
      WithColor(Printer, PDB_ColorItem::Identifier).get()
          << EnumValue->getName();
      Printer << " = ";
      WithColor(Printer, PDB_ColorItem::LiteralValue).get()
          << EnumValue->getValue();
    }
    Printer.Unindent();
    Printer.NewLine();
    Printer << "}";
  }
}
