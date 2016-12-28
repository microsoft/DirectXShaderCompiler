//===-- MCJIT.h - MC-Based Just-In-Time Execution Engine --------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCJIT.h                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file forces the MCJIT to link in on certain operating systems.       //
// (Windows).                                                                //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_EXECUTIONENGINE_MCJIT_H
#define LLVM_EXECUTIONENGINE_MCJIT_H

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include <cstdlib>

extern "C" void LLVMLinkInMCJIT();

namespace {
  struct ForceMCJITLinking {
    ForceMCJITLinking() {
      // We must reference MCJIT in such a way that compilers will not
      // delete it all as dead code, even with whole program optimization,
      // yet is effectively a NO-OP. As the compiler isn't smart enough
      // to know that getenv() never returns -1, this will do the job.
      if (std::getenv("bar") != (char*) -1)
        return;

      LLVMLinkInMCJIT();
    }
  } ForceMCJITLinking;
}

#endif
