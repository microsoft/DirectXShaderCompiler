//===---- Watchdog.cpp - Implement Watchdog ---------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Watchdog.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file implements the Watchdog class.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/Watchdog.h"
#include "llvm/Config/config.h"

// Include the platform-specific parts of this class.
#ifdef LLVM_ON_UNIX
#include "Unix/Watchdog.inc"
#endif
#ifdef LLVM_ON_WIN32
#include "Windows/Watchdog.inc"
#endif
