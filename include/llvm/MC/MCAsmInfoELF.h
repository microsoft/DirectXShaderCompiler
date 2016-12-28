//===-- llvm/MC/MCAsmInfoELF.h - ELF Asm info -------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmInfoELF.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCASMINFOELF_H
#define LLVM_MC_MCASMINFOELF_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
class MCAsmInfoELF : public MCAsmInfo {
  virtual void anchor();
  MCSection *getNonexecutableStackSection(MCContext &Ctx) const final;

protected:
  MCAsmInfoELF();
};
}

#endif
