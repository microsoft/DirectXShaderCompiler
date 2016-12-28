//===---- MCAsmInfoDarwin.h - Darwin asm properties -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmInfoDarwin.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines target asm properties related what form asm statements  //
// should take in general on Darwin-based targets                            //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCASMINFODARWIN_H
#define LLVM_MC_MCASMINFODARWIN_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
  class MCAsmInfoDarwin : public MCAsmInfo {
  public:
    explicit MCAsmInfoDarwin();
    bool isSectionAtomizableBySymbols(const MCSection &Section) const override;
  };
}


#endif // LLVM_MC_MCASMINFODARWIN_H
