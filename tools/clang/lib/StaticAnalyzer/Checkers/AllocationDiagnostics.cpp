//=- AllocationDiagnostics.cpp - Config options for allocation diags *- C++ -*-//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AllocationDiagnostics.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Declares the configuration functions for leaks/allocation diagnostics.    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "AllocationDiagnostics.h"

namespace clang {
namespace ento {

bool shouldIncludeAllocationSiteInLeakDiagnostics(AnalyzerOptions &AOpts) {
  return AOpts.getBooleanOption("leak-diagnostics-reference-allocation",
                                false);
}

}}
