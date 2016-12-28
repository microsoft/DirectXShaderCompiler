//=--- AllocationDiagnostics.h - Config options for allocation diags *- C++ -*-//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AllocationDiagnostics.h                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Declares the configuration functions for leaks/allocation diagnostics.    //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_ALLOCATIONDIAGNOSTICS_H
#define LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_ALLOCATIONDIAGNOSTICS_H

#include "clang/StaticAnalyzer/Core/AnalyzerOptions.h"

namespace clang { namespace ento {

/// \brief Returns true if leak diagnostics should directly reference
/// the allocatin site (where possible).
///
/// The default is false.
///
bool shouldIncludeAllocationSiteInLeakDiagnostics(AnalyzerOptions &AOpts);

}}

#endif

