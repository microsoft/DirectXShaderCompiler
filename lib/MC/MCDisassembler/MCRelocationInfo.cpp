//==-- MCRelocationInfo.cpp ------------------------------------------------==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCRelocationInfo.cpp                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCRelocationInfo.h"
#include "llvm-c/Disassembler.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

MCRelocationInfo::MCRelocationInfo(MCContext &Ctx)
  : Ctx(Ctx) {
}

MCRelocationInfo::~MCRelocationInfo() {
}

const MCExpr *
MCRelocationInfo::createExprForRelocation(object::RelocationRef Rel) {
  return nullptr;
}

const MCExpr *
MCRelocationInfo::createExprForCAPIVariantKind(const MCExpr *SubExpr,
                                               unsigned VariantKind) {
  if (VariantKind != LLVMDisassembler_VariantKind_None)
    return nullptr;
  return SubExpr;
}

MCRelocationInfo *llvm::createMCRelocationInfo(const Triple &TT,
                                               MCContext &Ctx) {
  return new MCRelocationInfo(Ctx);
}
