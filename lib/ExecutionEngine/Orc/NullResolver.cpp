//===---------- NullResolver.cpp - Reject symbol lookup requests ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// NullResolver.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ExecutionEngine/Orc/NullResolver.h"

#include "llvm/Support/ErrorHandling.h"

namespace llvm {
namespace orc {

RuntimeDyld::SymbolInfo NullResolver::findSymbol(const std::string &Name) {
  llvm_unreachable("Unexpected cross-object symbol reference");
}

RuntimeDyld::SymbolInfo
NullResolver::findSymbolInLogicalDylib(const std::string &Name) {
  llvm_unreachable("Unexpected cross-object symbol reference");
}

} // End namespace orc.
} // End namespace llvm.
