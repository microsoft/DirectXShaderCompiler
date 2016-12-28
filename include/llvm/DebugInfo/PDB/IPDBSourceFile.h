//===- IPDBSourceFile.h - base interface for a PDB source file --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// IPDBSourceFile.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_IPDBSOURCEFILE_H
#define LLVM_DEBUGINFO_PDB_IPDBSOURCEFILE_H

#include "PDBTypes.h"
#include <memory>
#include <string>

namespace llvm {

class raw_ostream;

/// IPDBSourceFile defines an interface used to represent source files whose
/// information are stored in the PDB.
class IPDBSourceFile {
public:
  virtual ~IPDBSourceFile();

  void dump(raw_ostream &OS, int Indent) const;

  virtual std::string getFileName() const = 0;
  virtual uint32_t getUniqueId() const = 0;
  virtual std::string getChecksum() const = 0;
  virtual PDB_Checksum getChecksumType() const = 0;
  virtual std::unique_ptr<IPDBEnumSymbols> getCompilands() const = 0;
};
}

#endif
