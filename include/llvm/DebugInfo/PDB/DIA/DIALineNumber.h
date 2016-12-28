//===- DIALineNumber.h - DIA implementation of IPDBLineNumber ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIALineNumber.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIALINENUMBER_H
#define LLVM_DEBUGINFO_PDB_DIA_DIALINENUMBER_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBLineNumber.h"

namespace llvm {
class DIALineNumber : public IPDBLineNumber {
public:
  explicit DIALineNumber(CComPtr<IDiaLineNumber> DiaLineNumber);

  uint32_t getLineNumber() const override;
  uint32_t getLineNumberEnd() const override;
  uint32_t getColumnNumber() const override;
  uint32_t getColumnNumberEnd() const override;
  uint32_t getAddressSection() const override;
  uint32_t getAddressOffset() const override;
  uint32_t getRelativeVirtualAddress() const override;
  uint64_t getVirtualAddress() const override;
  uint32_t getLength() const override;
  uint32_t getSourceFileId() const override;
  uint32_t getCompilandId() const override;
  bool isStatement() const override;

private:
  CComPtr<IDiaLineNumber> LineNumber;
};
}

#endif
