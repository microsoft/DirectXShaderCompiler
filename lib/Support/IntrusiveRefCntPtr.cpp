//== IntrusiveRefCntPtr.cpp - Smart Refcounting Pointer ----------*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// IntrusiveRefCntPtr.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/IntrusiveRefCntPtr.h"

using namespace llvm;

void RefCountedBaseVPTR::anchor() { }
