//==- DIAEnumDebugStreams.h - DIA Debug Stream Enumerator impl ---*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIAEnumDebugStreams.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_DIA_DIAENUMDEBUGSTREAMS_H
#define LLVM_DEBUGINFO_PDB_DIA_DIAENUMDEBUGSTREAMS_H

#include "DIASupport.h"
#include "llvm/DebugInfo/PDB/IPDBEnumChildren.h"

namespace llvm {

class IPDBDataStream;

class DIAEnumDebugStreams : public IPDBEnumChildren<IPDBDataStream> {
public:
  explicit DIAEnumDebugStreams(CComPtr<IDiaEnumDebugStreams> DiaEnumerator);

  uint32_t getChildCount() const override;
  ChildTypePtr getChildAtIndex(uint32_t Index) const override;
  ChildTypePtr getNext() override;
  void reset() override;
  DIAEnumDebugStreams *clone() const override;

private:
  CComPtr<IDiaEnumDebugStreams> Enumerator;
};
}

#endif
