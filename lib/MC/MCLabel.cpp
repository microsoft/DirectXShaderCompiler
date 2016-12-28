//===- lib/MC/MCLabel.cpp - MCLabel implementation ----------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCLabel.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCLabel.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

void MCLabel::print(raw_ostream &OS) const {
  OS << '"' << getInstance() << '"';
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
void MCLabel::dump() const {
  print(dbgs());
}
#endif
