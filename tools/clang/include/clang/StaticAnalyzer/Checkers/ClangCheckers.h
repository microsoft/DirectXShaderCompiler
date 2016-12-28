//===--- ClangCheckers.h - Provides builtin checkers ------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ClangCheckers.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CHECKERS_CLANGCHECKERS_H
#define LLVM_CLANG_STATICANALYZER_CHECKERS_CLANGCHECKERS_H

namespace clang {
namespace ento {
class CheckerRegistry;

void registerBuiltinCheckers(CheckerRegistry &registry);

} // end namespace ento
} // end namespace clang

#endif
