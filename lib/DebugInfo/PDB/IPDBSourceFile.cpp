//===- IPDBSourceFile.cpp - base interface for a PDB source file *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// IPDBSourceFile.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/DebugInfo/PDB/IPDBSourceFile.h"

#include "llvm/DebugInfo/PDB/PDBExtras.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

IPDBSourceFile::~IPDBSourceFile() {}

void IPDBSourceFile::dump(raw_ostream &OS, int Indent) const {
  OS.indent(Indent);
  PDB_Checksum ChecksumType = getChecksumType();
  OS << "[";
  if (ChecksumType != PDB_Checksum::None) {
    OS << ChecksumType << ": ";
    std::string Checksum = getChecksum();
    for (uint8_t c : Checksum)
      OS << format_hex_no_prefix(c, 2, true);
  } else
    OS << "No checksum";
  OS << "] " << getFileName() << "\n";
}
