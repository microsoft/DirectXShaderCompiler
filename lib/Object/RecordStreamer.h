//===-- RecordStreamer.h - Record asm defined and used symbols ---*- C++ -*===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RecordStreamer.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_OBJECT_RECORDSTREAMER_H
#define LLVM_LIB_OBJECT_RECORDSTREAMER_H

#include "llvm/MC/MCStreamer.h"

namespace llvm {
class RecordStreamer : public MCStreamer {
public:
  enum State { NeverSeen, Global, Defined, DefinedGlobal, Used };

private:
  StringMap<State> Symbols;
  void markDefined(const MCSymbol &Symbol);
  void markGlobal(const MCSymbol &Symbol);
  void markUsed(const MCSymbol &Symbol);
  void visitUsedSymbol(const MCSymbol &Sym) override;

public:
  typedef StringMap<State>::const_iterator const_iterator;
  const_iterator begin();
  const_iterator end();
  RecordStreamer(MCContext &Context);
  void EmitInstruction(const MCInst &Inst, const MCSubtargetInfo &STI) override;
  void EmitLabel(MCSymbol *Symbol) override;
  void EmitAssignment(MCSymbol *Symbol, const MCExpr *Value) override;
  bool EmitSymbolAttribute(MCSymbol *Symbol, MCSymbolAttr Attribute) override;
  void EmitZerofill(MCSection *Section, MCSymbol *Symbol, uint64_t Size,
                    unsigned ByteAlignment) override;
  void EmitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                        unsigned ByteAlignment) override;
};
}
#endif
