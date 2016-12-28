//==- LocalCheckers.h - Intra-Procedural+Flow-Sensitive Checkers -*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LocalCheckers.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the interface to call a set of intra-procedural (local)//
//  checkers that use flow/path-sensitive analyses to find bugs.             //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CHECKERS_LOCALCHECKERS_H
#define LLVM_CLANG_STATICANALYZER_CHECKERS_LOCALCHECKERS_H

namespace clang {
namespace ento {

class ExprEngine;

void RegisterCallInliner(ExprEngine &Eng);

} // end namespace ento
} // end namespace clang

#endif
