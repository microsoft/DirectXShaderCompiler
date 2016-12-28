//=- PHIEliminationUtils.h - Helper functions for PHI elimination -*- C++ -*-=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PHIEliminationUtils.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_CODEGEN_PHIELIMINATIONUTILS_H
#define LLVM_LIB_CODEGEN_PHIELIMINATIONUTILS_H

#include "llvm/CodeGen/MachineBasicBlock.h"

namespace llvm {
    /// findPHICopyInsertPoint - Find a safe place in MBB to insert a copy from
    /// SrcReg when following the CFG edge to SuccMBB. This needs to be after
    /// any def of SrcReg, but before any subsequent point where control flow
    /// might jump out of the basic block.
    MachineBasicBlock::iterator
    findPHICopyInsertPoint(MachineBasicBlock* MBB, MachineBasicBlock* SuccMBB,
                           unsigned SrcReg);
}

#endif
