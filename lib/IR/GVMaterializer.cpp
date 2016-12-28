//===-- GVMaterializer.cpp - Base implementation for GV materializers -----===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// GVMaterializer.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Minimal implementation of the abstract interface for materializing        //
// GlobalValues.                                                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/GVMaterializer.h"
using namespace llvm;

GVMaterializer::~GVMaterializer() {}
