//===-- MCCodeGenInfo.cpp - Target CodeGen Info -----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCCodeGenInfo.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file tracks information about the target which can affect codegen,   //
// asm parsing, and asm printing. For example, relocation model.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCCodeGenInfo.h"
using namespace llvm;

void MCCodeGenInfo::initMCCodeGenInfo(Reloc::Model RM, CodeModel::Model CM,
                                      CodeGenOpt::Level OL) {
  RelocationModel = RM;
  CMModel = CM;
  OptLevel = OL;
}
