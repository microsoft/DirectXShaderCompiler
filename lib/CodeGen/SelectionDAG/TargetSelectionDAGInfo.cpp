//===-- TargetSelectionDAGInfo.cpp - SelectionDAG Info --------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TargetSelectionDAGInfo.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This implements the TargetSelectionDAGInfo class.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Target/TargetSelectionDAGInfo.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

TargetSelectionDAGInfo::~TargetSelectionDAGInfo() {
}
