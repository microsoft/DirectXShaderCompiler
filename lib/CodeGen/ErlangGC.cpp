//===-- ErlangGC.cpp - Erlang/OTP GC strategy -------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ErlangGC.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Erlang/OTP runtime-compatible garbage collector  //
// (e.g. defines safe points, root initialization etc.)                      //
//                                                                           //
// The frametable emitter is in ErlangGCPrinter.cpp.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/CodeGen/GCs.h"
#include "llvm/CodeGen/GCStrategy.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;

namespace {

class ErlangGC : public GCStrategy {
public:
  ErlangGC();
};
}

static GCRegistry::Add<ErlangGC> X("erlang",
                                   "erlang-compatible garbage collector");

void llvm::linkErlangGC() {}

ErlangGC::ErlangGC() {
  InitRoots = false;
  NeededSafePoints = 1 << GC::PostCall;
  UsesMetadata = true;
  CustomRoots = false;
}
