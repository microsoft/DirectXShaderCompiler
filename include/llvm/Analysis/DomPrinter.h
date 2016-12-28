//===-- DomPrinter.h - Dom printer external interface ------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DomPrinter.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines external functions that can be called to explicitly     //
// instantiate the dominance tree printer.                                   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_ANALYSIS_DOMPRINTER_H
#define LLVM_ANALYSIS_DOMPRINTER_H

namespace llvm {
  class FunctionPass;
  FunctionPass *createDomPrinterPass();
  FunctionPass *createDomOnlyPrinterPass();
  FunctionPass *createDomViewerPass();
  FunctionPass *createDomOnlyViewerPass();
  FunctionPass *createPostDomPrinterPass();
  FunctionPass *createPostDomOnlyPrinterPass();
  FunctionPass *createPostDomViewerPass();
  FunctionPass *createPostDomOnlyViewerPass();
} // End llvm namespace

#endif
