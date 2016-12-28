//==--- InterCheckerAPI.h ---------------------------------------*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// InterCheckerAPI.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file allows introduction of checker dependencies. It contains APIs for//
// inter-checker communications.                                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_INTERCHECKERAPI_H
#define LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_INTERCHECKERAPI_H
namespace clang {
class CheckerManager;

namespace ento {

/// Register the checker which evaluates CString API calls.
void registerCStringCheckerBasic(CheckerManager &Mgr);

}}
#endif /* INTERCHECKERAPI_H_ */
