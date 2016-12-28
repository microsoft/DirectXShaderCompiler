//===-- TargetIntrinsicInfo.cpp - Target Instruction Information ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TargetIntrinsicInfo.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the TargetIntrinsicInfo class.                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Target/TargetIntrinsicInfo.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Function.h"
using namespace llvm;

TargetIntrinsicInfo::TargetIntrinsicInfo() {
}

TargetIntrinsicInfo::~TargetIntrinsicInfo() {
}

unsigned TargetIntrinsicInfo::getIntrinsicID(Function *F) const {
  const ValueName *ValName = F->getValueName();
  if (!ValName)
    return 0;
  return lookupName(ValName->getKeyData(), ValName->getKeyLength());
}
