//===--- DiagnosticOptions.cpp - C Language Family Diagnostic Handling ----===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DiagnosticOptions.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file implements the DiagnosticOptions related interfaces.           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {

raw_ostream& operator<<(raw_ostream& Out, DiagnosticLevelMask M) {
  using UT = std::underlying_type<DiagnosticLevelMask>::type;
  return Out << static_cast<UT>(M);
}

} // end namespace clang
