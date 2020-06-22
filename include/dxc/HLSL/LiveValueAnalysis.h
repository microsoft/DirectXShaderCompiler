///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LiveValueAnalysis.h                                                       //
// Copyright (C) 2020 NVIDIA Corporation.  All rights reserved.              //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Live Value Analysis pass for HLSL.                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
  class ModulePass;
  class PassRegistry;
  class StringRef;

  /// \brief Create and return a pass that reports live values around TraceRay call sites.
  /// Note that this pass is designed for use with the legacy pass manager.
  ModulePass *createLiveValueAnalysisPass(StringRef LiveValueAnalysisOutputFile);
  void initializeLiveValueAnalysisPass(llvm::PassRegistry&);
}