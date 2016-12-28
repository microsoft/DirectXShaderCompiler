//===- llvm/unittest/ADT/SparseBitVectorTest.cpp - SparseBitVector tests --===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SparseBitVectorTest.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/SparseBitVector.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(SparseBitVectorTest, TrivialOperation) {
  SparseBitVector<> Vec;
  EXPECT_EQ(0U, Vec.count());
  EXPECT_FALSE(Vec.test(17));
  Vec.set(5);
  EXPECT_TRUE(Vec.test(5));
  EXPECT_FALSE(Vec.test(17));
  Vec.reset(6);
  EXPECT_TRUE(Vec.test(5));
  EXPECT_FALSE(Vec.test(6));
  Vec.reset(5);
  EXPECT_FALSE(Vec.test(5));
  EXPECT_TRUE(Vec.test_and_set(17));
  EXPECT_FALSE(Vec.test_and_set(17));
  EXPECT_TRUE(Vec.test(17));
  Vec.clear();
  EXPECT_FALSE(Vec.test(17));
}

}
