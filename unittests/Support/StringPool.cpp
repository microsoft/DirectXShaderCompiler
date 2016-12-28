//===- llvm/unittest/Support/StringPoiil.cpp - StringPool tests -----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// StringPool.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/StringPool.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(PooledStringPtrTest, OperatorEquals) {
  StringPool pool;
  const PooledStringPtr a = pool.intern("a");
  const PooledStringPtr b = pool.intern("b");
  EXPECT_FALSE(a == b);
}

TEST(PooledStringPtrTest, OperatorNotEquals) {
  StringPool pool;
  const PooledStringPtr a = pool.intern("a");
  const PooledStringPtr b = pool.intern("b");
  EXPECT_TRUE(a != b);
}

}
