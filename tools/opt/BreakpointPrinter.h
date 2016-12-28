//===- BreakpointPrinter.h - Breakpoint location printer ------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// BreakpointPrinter.h                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Breakpoint location printer.                                      //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef LLVM_TOOLS_OPT_BREAKPOINTPRINTER_H
#define LLVM_TOOLS_OPT_BREAKPOINTPRINTER_H

namespace llvm {

class ModulePass;
class raw_ostream;

ModulePass *createBreakpointPrinter(raw_ostream &out);
}

#endif // LLVM_TOOLS_OPT_BREAKPOINTPRINTER_H
