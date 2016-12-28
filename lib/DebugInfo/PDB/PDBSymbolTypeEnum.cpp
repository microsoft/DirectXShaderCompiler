//===- PDBSymbolTypeEnum.cpp - --------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PDBSymbolTypeEnum.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/DebugInfo/PDB/PDBSymbolTypeEnum.h"

#include "llvm/DebugInfo/PDB/IPDBSession.h"
#include "llvm/DebugInfo/PDB/PDBSymDumper.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeBuiltin.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeUDT.h"

#include <utility>

using namespace llvm;

PDBSymbolTypeEnum::PDBSymbolTypeEnum(const IPDBSession &PDBSession,
                                     std::unique_ptr<IPDBRawSymbol> Symbol)
    : PDBSymbol(PDBSession, std::move(Symbol)) {}

std::unique_ptr<PDBSymbolTypeUDT> PDBSymbolTypeEnum::getClassParent() const {
  return Session.getConcreteSymbolById<PDBSymbolTypeUDT>(getClassParentId());
}

std::unique_ptr<PDBSymbolTypeBuiltin>
PDBSymbolTypeEnum::getUnderlyingType() const {
  return Session.getConcreteSymbolById<PDBSymbolTypeBuiltin>(getTypeId());
}

void PDBSymbolTypeEnum::dump(PDBSymDumper &Dumper) const { Dumper.dump(*this); }
