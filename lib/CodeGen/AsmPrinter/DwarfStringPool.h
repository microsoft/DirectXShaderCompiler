//===-- llvm/CodeGen/DwarfStringPool.h - Dwarf Debug Framework -*- C++ -*--===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DwarfStringPool.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_CODEGEN_ASMPRINTER_DWARFSTRINGPOOL_H
#define LLVM_LIB_CODEGEN_ASMPRINTER_DWARFSTRINGPOOL_H

#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/DwarfStringPoolEntry.h"
#include "llvm/Support/Allocator.h"
#include <utility>

namespace llvm {

class AsmPrinter;
class MCSymbol;
class MCSection;
class StringRef;

// Collection of strings for this unit and assorted symbols.
// A String->Symbol mapping of strings used by indirect
// references.
class DwarfStringPool {
  typedef DwarfStringPoolEntry EntryTy;
  StringMap<EntryTy, BumpPtrAllocator &> Pool;
  StringRef Prefix;
  unsigned NumBytes = 0;
  bool ShouldCreateSymbols;

public:
  typedef DwarfStringPoolEntryRef EntryRef;

  DwarfStringPool(BumpPtrAllocator &A, AsmPrinter &Asm, StringRef Prefix);

  void emit(AsmPrinter &Asm, MCSection *StrSection,
            MCSection *OffsetSection = nullptr);

  bool empty() const { return Pool.empty(); }

  /// Get a reference to an entry in the string pool.
  EntryRef getEntry(AsmPrinter &Asm, StringRef Str);
};
}
#endif
