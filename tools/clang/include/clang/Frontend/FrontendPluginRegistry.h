//===-- FrontendAction.h - Pluggable Frontend Action Interface --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FrontendPluginRegistry.h                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_FRONTEND_FRONTENDPLUGINREGISTRY_H
#define LLVM_CLANG_FRONTEND_FRONTENDPLUGINREGISTRY_H

#include "clang/Frontend/FrontendAction.h"
#include "llvm/Support/Registry.h"

// Instantiated in FrontendAction.cpp.
extern template class llvm::Registry<clang::PluginASTAction>;

namespace clang {

/// The frontend plugin registry.
typedef llvm::Registry<PluginASTAction> FrontendPluginRegistry;

} // end namespace clang

#endif
