//===- MIRPrinter.h - MIR serialization format printer --------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MIRPrinter.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares the functions that print out the LLVM IR and the machine//
// functions using the MIR serialization format.                             //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_CODEGEN_MIRPRINTER_H
#define LLVM_LIB_CODEGEN_MIRPRINTER_H

namespace llvm {

class MachineFunction;
class Module;
class raw_ostream;

/// Print LLVM IR using the MIR serialization format to the given output stream.
void printMIR(raw_ostream &OS, const Module &M);

/// Print a machine function using the MIR serialization format to the given
/// output stream.
void printMIR(raw_ostream &OS, const MachineFunction &MF);

} // end namespace llvm

#endif
