//===-- COM.cpp - Implement COM utility classes -----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// COM.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file implements utility classes related to COM.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/COM.h"

#include "llvm/Config/config.h"

// Include the platform-specific parts of this class.
#ifdef LLVM_ON_UNIX
#include "Unix/COM.inc"
#elif LLVM_ON_WIN32
#include "Windows/COM.inc"
#endif
