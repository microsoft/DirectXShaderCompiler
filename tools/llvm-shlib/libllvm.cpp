//===-libllvm.cpp - LLVM Shared Library -----------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// libllvm.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file is empty and serves only the purpose of making CMake happy because//
// you can't define a target with no sources.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Config/config.h"

#if defined(DISABLE_LLVM_DYLIB_ATEXIT)
extern "C" int __cxa_atexit();
extern "C" int __cxa_atexit() { return 0; }
#endif
