//===-- MCAsmInfoCOFF.h - COFF asm properties -------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmInfoCOFF.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_MC_MCASMINFOCOFF_H
#define LLVM_MC_MCASMINFOCOFF_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
  class MCAsmInfoCOFF : public MCAsmInfo {
    virtual void anchor();
  protected:
    explicit MCAsmInfoCOFF();
  };

  class MCAsmInfoMicrosoft : public MCAsmInfoCOFF {
    void anchor() override;
  protected:
    explicit MCAsmInfoMicrosoft();
  };

  class MCAsmInfoGNUCOFF : public MCAsmInfoCOFF {
    void anchor() override;
  protected:
    explicit MCAsmInfoGNUCOFF();
  };
}


#endif // LLVM_MC_MCASMINFOCOFF_H
