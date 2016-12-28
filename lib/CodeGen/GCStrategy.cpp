//===-- GCStrategy.cpp - Garbage Collector Description --------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// GCStrategy.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the policy object GCStrategy which describes the     //
// behavior of a given garbage collector.                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/CodeGen/GCStrategy.h"

using namespace llvm;

GCStrategy::GCStrategy()
    : UseStatepoints(false), NeededSafePoints(0), CustomReadBarriers(false),
      CustomWriteBarriers(false), CustomRoots(false), InitRoots(true),
      UsesMetadata(false) {}
