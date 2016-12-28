//===--- CompilationDatabasePluginRegistry.h - ------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CompilationDatabasePluginRegistry.h                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_TOOLING_COMPILATIONDATABASEPLUGINREGISTRY_H
#define LLVM_CLANG_TOOLING_COMPILATIONDATABASEPLUGINREGISTRY_H

#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/Support/Registry.h"

namespace clang {
namespace tooling {

class CompilationDatabasePlugin;

typedef llvm::Registry<CompilationDatabasePlugin>
    CompilationDatabasePluginRegistry;

} // end namespace tooling
} // end namespace clang

#endif
