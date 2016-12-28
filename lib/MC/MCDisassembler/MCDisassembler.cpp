//===-- MCDisassembler.cpp - Disassembler interface -----------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCDisassembler.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCExternalSymbolizer.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

MCDisassembler::~MCDisassembler() {
}

bool MCDisassembler::tryAddingSymbolicOperand(MCInst &Inst, int64_t Value,
                                              uint64_t Address, bool IsBranch,
                                              uint64_t Offset,
                                              uint64_t InstSize) const {
  raw_ostream &cStream = CommentStream ? *CommentStream : nulls();
  if (Symbolizer)
    return Symbolizer->tryAddingSymbolicOperand(Inst, cStream, Value, Address,
                                                IsBranch, Offset, InstSize);
  return false;
}

void MCDisassembler::tryAddingPcLoadReferenceComment(int64_t Value,
                                                     uint64_t Address) const {
  raw_ostream &cStream = CommentStream ? *CommentStream : nulls();
  if (Symbolizer)
    Symbolizer->tryAddingPcLoadReferenceComment(cStream, Value, Address);
}

void MCDisassembler::setSymbolizer(std::unique_ptr<MCSymbolizer> Symzer) {
  Symbolizer = std::move(Symzer);
}
