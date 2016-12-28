//===-- DWARFRelocMap.h -----------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DWARFRelocMap.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_LIB_DEBUGINFO_DWARFRELOCMAP_H
#define LLVM_LIB_DEBUGINFO_DWARFRELOCMAP_H

#include "llvm/ADT/DenseMap.h"

namespace llvm {

typedef DenseMap<uint64_t, std::pair<uint8_t, int64_t> > RelocAddrMap;

} // namespace llvm

#endif

