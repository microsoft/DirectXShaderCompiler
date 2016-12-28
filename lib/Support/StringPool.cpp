//===-- StringPool.cpp - Interned string pool -----------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// StringPool.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the StringPool class.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/StringPool.h"
#include "llvm/ADT/StringRef.h"

using namespace llvm;

StringPool::StringPool() {}

StringPool::~StringPool() {
  assert(InternTable.empty() && "PooledStringPtr leaked!");
}

PooledStringPtr StringPool::intern(StringRef Key) {
  table_t::iterator I = InternTable.find(Key);
  if (I != InternTable.end())
    return PooledStringPtr(&*I);
  
  entry_t *S = entry_t::Create(Key);
  S->getValue().Pool = this;
  InternTable.insert(S);
  
  return PooledStringPtr(S);
}
