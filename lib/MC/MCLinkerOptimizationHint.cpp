//===-- llvm/MC/MCLinkerOptimizationHint.cpp ----- LOH handling -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCLinkerOptimizationHint.cpp                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCLinkerOptimizationHint.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/Support/LEB128.h"

using namespace llvm;

// Each LOH is composed by, in this order (each field is encoded using ULEB128):
// - Its kind.
// - Its number of arguments (let say N).
// - Its arg1.
// - ...
// - Its argN.
// <arg1> to <argN> are absolute addresses in the object file, i.e.,
// relative addresses from the beginning of the object file.
void MCLOHDirective::emit_impl(raw_ostream &OutStream,
                               const MachObjectWriter &ObjWriter,
                               const MCAsmLayout &Layout) const {
  encodeULEB128(Kind, OutStream);
  encodeULEB128(Args.size(), OutStream);
  for (LOHArgs::const_iterator It = Args.begin(), EndIt = Args.end();
       It != EndIt; ++It)
    encodeULEB128(ObjWriter.getSymbolAddress(**It, Layout), OutStream);
}
