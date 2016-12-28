//===- SymbolSize.h ---------------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SymbolSize.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_OBJECT_SYMBOLSIZE_H
#define LLVM_OBJECT_SYMBOLSIZE_H

#include "llvm/Object/ObjectFile.h"

namespace llvm {
namespace object {
std::vector<std::pair<SymbolRef, uint64_t>>
computeSymbolSizes(const ObjectFile &O);
}
} // namespace llvm

#endif
