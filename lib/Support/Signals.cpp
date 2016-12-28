//===- Signals.cpp - Signal Handling support --------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Signals.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines some helpful functions for dealing with the possibility of//
// Unix signals occurring while your program is running.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/Signals.h"
#include "llvm/Config/config.h"

namespace llvm {
using namespace sys;

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

}

// Include the platform-specific parts of this class.
#ifdef LLVM_ON_UNIX
#include "Unix/Signals.inc"
#endif
#ifdef LLVM_ON_WIN32
#include "Windows/Signals.inc"
#endif
