//===- llvm/Transforms/Utils/UnrollLoop.h - Unrolling utilities -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// UnrollLoop.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines some loop unrolling utilities. It does not define any   //
// actual pass or policy, but provides a single function to perform loop     //
// unrolling.                                                                //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TRANSFORMS_UTILS_UNROLLLOOP_H
#define LLVM_TRANSFORMS_UTILS_UNROLLLOOP_H

#include "llvm/ADT/StringRef.h"

namespace llvm {

class AssumptionCache;
class Loop;
class LoopInfo;
class LPPassManager;
class MDNode;
class Pass;

bool UnrollLoop(Loop *L, unsigned Count, unsigned TripCount, bool AllowRuntime,
                bool AllowExpensiveTripCount, unsigned TripMultiple,
                LoopInfo *LI, Pass *PP, LPPassManager *LPM,
                AssumptionCache *AC);

bool UnrollRuntimeLoopProlog(Loop *L, unsigned Count,
                             bool AllowExpensiveTripCount, LoopInfo *LI,
                             LPPassManager *LPM);

MDNode *GetUnrollMetadata(MDNode *LoopID, StringRef Name);
}

#endif
