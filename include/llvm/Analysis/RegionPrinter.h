//===-- RegionPrinter.h - Region printer external interface -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RegionPrinter.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines external functions that can be called to explicitly     //
// instantiate the region printer.                                           //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_ANALYSIS_REGIONPRINTER_H
#define LLVM_ANALYSIS_REGIONPRINTER_H

namespace llvm {
  class FunctionPass;
  FunctionPass *createRegionViewerPass();
  FunctionPass *createRegionOnlyViewerPass();
  FunctionPass *createRegionPrinterPass();
  FunctionPass *createRegionOnlyPrinterPass();
} // End llvm namespace

#endif
