//===--- Utils.h - Misc utilities for the front-end -------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Utils.h                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This header contains miscellaneous utilities for various front-end actions//
//  which were split from Frontend to minimise Frontend's dependencies.      //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_FRONTENDTOOL_UTILS_H
#define LLVM_CLANG_FRONTENDTOOL_UTILS_H

namespace clang {

class CompilerInstance;

/// ExecuteCompilerInvocation - Execute the given actions described by the
/// compiler invocation object in the given compiler instance.
///
/// \return - True on success.
bool ExecuteCompilerInvocation(CompilerInstance *Clang);

}  // end namespace clang

#endif
