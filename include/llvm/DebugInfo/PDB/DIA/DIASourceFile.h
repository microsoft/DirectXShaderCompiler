//===- DIASourceFile.h - DIA implementation of IPDBSourceFile ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIASourceFile.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIASOURCEFILE_H
#define LLVM_DEBUGINFO_PDB_DIA_DIASOURCEFILE_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBSourceFile.h"

namespace llvm {
class DIASession;

class DIASourceFile : public IPDBSourceFile {
public:
  explicit DIASourceFile(const DIASession &Session,
                         CComPtr<IDiaSourceFile> DiaSourceFile);

  std::string getFileName() const override;
  uint32_t getUniqueId() const override;
  std::string getChecksum() const override;
  PDB_Checksum getChecksumType() const override;
  std::unique_ptr<IPDBEnumSymbols> getCompilands() const override;

private:
  const DIASession &Session;
  CComPtr<IDiaSourceFile> SourceFile;
};
}

#endif
