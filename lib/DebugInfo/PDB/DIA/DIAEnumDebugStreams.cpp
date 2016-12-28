//==- DIAEnumDebugStreams.cpp - DIA Debug Stream Enumerator impl -*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIAEnumDebugStreams.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/DebugInfo/PDB/PDBSymbol.h"
#include "llvm/DebugInfo/PDB/DIA/DIADataStream.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumDebugStreams.h"

using namespace llvm;

DIAEnumDebugStreams::DIAEnumDebugStreams(
    CComPtr<IDiaEnumDebugStreams> DiaEnumerator)
    : Enumerator(DiaEnumerator) {}

uint32_t DIAEnumDebugStreams::getChildCount() const {
  LONG Count = 0;
  return (S_OK == Enumerator->get_Count(&Count)) ? Count : 0;
}

std::unique_ptr<IPDBDataStream>
DIAEnumDebugStreams::getChildAtIndex(uint32_t Index) const {
  CComPtr<IDiaEnumDebugStreamData> Item;
  VARIANT VarIndex;
  VarIndex.vt = VT_I4;
  VarIndex.lVal = Index;
  if (S_OK != Enumerator->Item(VarIndex, &Item))
    return nullptr;

  return std::unique_ptr<IPDBDataStream>(new DIADataStream(Item));
}

std::unique_ptr<IPDBDataStream> DIAEnumDebugStreams::getNext() {
  CComPtr<IDiaEnumDebugStreamData> Item;
  ULONG NumFetched = 0;
  if (S_OK != Enumerator->Next(1, &Item, &NumFetched))
    return nullptr;

  return std::unique_ptr<IPDBDataStream>(new DIADataStream(Item));
}

void DIAEnumDebugStreams::reset() { Enumerator->Reset(); }

DIAEnumDebugStreams *DIAEnumDebugStreams::clone() const {
  CComPtr<IDiaEnumDebugStreams> EnumeratorClone;
  if (S_OK != Enumerator->Clone(&EnumeratorClone))
    return nullptr;
  return new DIAEnumDebugStreams(EnumeratorClone);
}
