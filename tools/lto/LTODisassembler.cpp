//===-- LTODisassembler.cpp - LTO Disassembler interface ------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LTODisassembler.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This function provides utility methods used by clients of libLTO that want//
// to use the disassembler.                                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm-c/lto.h"
#include "llvm/Support/TargetSelect.h"

using namespace llvm;

void lto_initialize_disassembler() {
  // Initialize targets and assembly printers/parsers.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllDisassemblers();
}
