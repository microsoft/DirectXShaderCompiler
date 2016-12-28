//===- Memory.cpp - Memory Handling Support ---------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Memory.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines some helpful functions for allocating memory and dealing//
// with memory mapped files                                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/Memory.h"
#include "llvm/Config/config.h"
#include "llvm/Support/Valgrind.h"

// Include the platform-specific parts of this class.
#ifdef LLVM_ON_UNIX
#include "Unix/Memory.inc"
#endif
#ifdef LLVM_ON_WIN32
#include "Windows/Memory.inc"
#endif
