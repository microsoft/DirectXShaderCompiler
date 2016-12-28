//==- DIAEnumLineNumbers.cpp - DIA Line Number Enumerator impl ---*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DIAEnumLineNumbers.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/DebugInfo/PDB/PDBSymbol.h"
#include "llvm/DebugInfo/PDB/DIA/DIAEnumLineNumbers.h"
#include "llvm/DebugInfo/PDB/DIA/DIALineNumber.h"

using namespace llvm;

DIAEnumLineNumbers::DIAEnumLineNumbers(
    CComPtr<IDiaEnumLineNumbers> DiaEnumerator)
    : Enumerator(DiaEnumerator) {}

uint32_t DIAEnumLineNumbers::getChildCount() const {
  LONG Count = 0;
  return (S_OK == Enumerator->get_Count(&Count)) ? Count : 0;
}

std::unique_ptr<IPDBLineNumber>
DIAEnumLineNumbers::getChildAtIndex(uint32_t Index) const {
  CComPtr<IDiaLineNumber> Item;
  if (S_OK != Enumerator->Item(Index, &Item))
    return nullptr;

  return std::unique_ptr<IPDBLineNumber>(new DIALineNumber(Item));
}

std::unique_ptr<IPDBLineNumber> DIAEnumLineNumbers::getNext() {
  CComPtr<IDiaLineNumber> Item;
  ULONG NumFetched = 0;
  if (S_OK != Enumerator->Next(1, &Item, &NumFetched))
    return nullptr;

  return std::unique_ptr<IPDBLineNumber>(new DIALineNumber(Item));
}

void DIAEnumLineNumbers::reset() { Enumerator->Reset(); }

DIAEnumLineNumbers *DIAEnumLineNumbers::clone() const {
  CComPtr<IDiaEnumLineNumbers> EnumeratorClone;
  if (S_OK != Enumerator->Clone(&EnumeratorClone))
    return nullptr;
  return new DIAEnumLineNumbers(EnumeratorClone);
}
