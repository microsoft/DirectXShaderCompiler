//===- DIADataStream.h - DIA implementation of IPDBDataStream ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIADataStream.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIADATASTREAM_H
#define LLVM_DEBUGINFO_PDB_DIA_DIADATASTREAM_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBDataStream.h"

namespace llvm {
class DIADataStream : public IPDBDataStream {
public:
  explicit DIADataStream(CComPtr<IDiaEnumDebugStreamData> DiaStreamData);

  uint32_t getRecordCount() const override;
  std::string getName() const override;
  llvm::Optional<RecordType> getItemAtIndex(uint32_t Index) const override;
  bool getNext(RecordType &Record) override;
  void reset() override;
  DIADataStream *clone() const override;

private:
  CComPtr<IDiaEnumDebugStreamData> StreamData;
};
}

#endif
