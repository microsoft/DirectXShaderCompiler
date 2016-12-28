//===- llvm/TableGen/Main.h - tblgen entry point ----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Main.h                                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares the common entry point for tblgen tools.               //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TABLEGEN_MAIN_H
#define LLVM_TABLEGEN_MAIN_H

#include <sal.h>  // HLSL Change - SAL

namespace llvm {

class RecordKeeper;
class raw_ostream;
/// \brief Perform the action using Records, and write output to OS.
/// \returns true on error, false otherwise
typedef bool TableGenMainFn(raw_ostream &OS, RecordKeeper &Records);

int TableGenMain(_In_z_ char *argv0, _In_ TableGenMainFn *MainFn);  // HLSL Change - SAL
}

#endif
