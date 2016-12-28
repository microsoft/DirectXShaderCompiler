//===-- DWARFSection.h ------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DWARFSection.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_DEBUGINFO_DWARFSECTION_H
#define LLVM_LIB_DEBUGINFO_DWARFSECTION_H

#include "llvm/DebugInfo/DWARF/DWARFRelocMap.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

struct DWARFSection {
  StringRef Data;
  RelocAddrMap Relocs;
};

}

#endif
