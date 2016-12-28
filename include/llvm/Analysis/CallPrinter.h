//===-- CallPrinter.h - Call graph printer external interface ----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CallPrinter.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines external functions that can be called to explicitly     //
// instantiate the call graph printer.                                       //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_ANALYSIS_CALLPRINTER_H
#define LLVM_ANALYSIS_CALLPRINTER_H

namespace llvm {

  class ModulePass;

  ModulePass *createCallGraphViewerPass();
  ModulePass *createCallGraphPrinterPass();

} // end namespace llvm

#endif
