//===- DebugLocStream.cpp - DWARF debug_loc stream --------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DebugLocStream.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DebugLocStream.h"
#include "DwarfDebug.h"
#include "llvm/CodeGen/AsmPrinter.h"

using namespace llvm;

bool DebugLocStream::finalizeList(AsmPrinter &Asm) {
  if (Lists.back().EntryOffset == Entries.size()) {
    // Empty list.  Delete it.
    Lists.pop_back();
    return false;
  }

  // Real list.  Generate a label for it.
  Lists.back().Label = Asm.createTempSymbol("debug_loc");
  return true;
}

void DebugLocStream::finalizeEntry() {
  if (Entries.back().ByteOffset != DWARFBytes.size())
    return;

  // The last entry was empty.  Delete it.
  Comments.erase(Comments.begin() + Entries.back().CommentOffset,
                 Comments.end());
  Entries.pop_back();

  assert(Lists.back().EntryOffset <= Entries.size() &&
         "Popped off more entries than are in the list");
}

DebugLocStream::ListBuilder::~ListBuilder() {
  if (!Locs.finalizeList(Asm))
    return;
  V.initializeDbgValue(&MI);
  V.setDebugLocListIndex(ListIndex);
}
