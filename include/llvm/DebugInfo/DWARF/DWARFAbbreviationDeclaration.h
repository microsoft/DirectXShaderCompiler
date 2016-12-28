//===-- DWARFAbbreviationDeclaration.h --------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DWARFAbbreviationDeclaration.h                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_DEBUGINFO_DWARFABBREVIATIONDECLARATION_H
#define LLVM_LIB_DEBUGINFO_DWARFABBREVIATIONDECLARATION_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/DataExtractor.h"

namespace llvm {

class raw_ostream;

class DWARFAbbreviationDeclaration {
public:
  struct AttributeSpec {
    AttributeSpec(uint16_t Attr, uint16_t Form) : Attr(Attr), Form(Form) {}
    uint16_t Attr;
    uint16_t Form;
  };
  typedef SmallVector<AttributeSpec, 8> AttributeSpecVector;

  DWARFAbbreviationDeclaration();

  uint32_t getCode() const { return Code; }
  uint32_t getTag() const { return Tag; }
  bool hasChildren() const { return HasChildren; }

  typedef iterator_range<AttributeSpecVector::const_iterator>
  attr_iterator_range;

  attr_iterator_range attributes() const {
    return attr_iterator_range(AttributeSpecs.begin(), AttributeSpecs.end());
  }

  uint16_t getFormByIndex(uint32_t idx) const {
    return idx < AttributeSpecs.size() ? AttributeSpecs[idx].Form : 0;
  }

  uint32_t findAttributeIndex(uint16_t attr) const;
  bool extract(DataExtractor Data, uint32_t* OffsetPtr);
  void dump(raw_ostream &OS) const;

private:
  void clear();

  uint32_t Code;
  uint32_t Tag;
  bool HasChildren;

  AttributeSpecVector AttributeSpecs;
};

}

#endif
