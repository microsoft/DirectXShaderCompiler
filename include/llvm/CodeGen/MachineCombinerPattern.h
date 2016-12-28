//===-- llvm/CodeGen/MachineCombinerPattern.h - Instruction pattern supported by
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MachineCombinerPattern.h                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines instruction pattern supported by combiner               //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CODEGEN_MACHINECOMBINERPATTERN_H
#define LLVM_CODEGEN_MACHINECOMBINERPATTERN_H

namespace llvm {

/// Enumeration of instruction pattern supported by machine combiner
///
///
namespace MachineCombinerPattern {
// Forward declaration
enum MC_PATTERN : int;
} // end namespace MachineCombinerPattern
} // end namespace llvm

#endif
