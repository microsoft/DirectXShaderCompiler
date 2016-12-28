//===--- Phases.cpp - Transformations on Driver Types ---------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Phases.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Driver/Phases.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>

using namespace clang::driver;

const char *phases::getPhaseName(ID Id) {
  switch (Id) {
  case Preprocess: return "preprocessor";
  case Precompile: return "precompiler";
  case Compile: return "compiler";
  case Backend: return "backend";
  case Assemble: return "assembler";
  case Link: return "linker";
  }

  llvm_unreachable("Invalid phase id.");
}
