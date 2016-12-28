//===- llvm/unittest/ADT/FoldingSetTest.cpp -------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FoldingSet.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// FoldingSet unit tests.                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "llvm/ADT/FoldingSet.h"
#include <string>

using namespace llvm;

namespace {

// Unaligned string test.
TEST(FoldingSetTest, UnalignedStringTest) {
  SCOPED_TRACE("UnalignedStringTest");

  FoldingSetNodeID a, b;
  // An aligned string
  std::string str1= "a test string";
  a.AddString(str1);

  // An unaligned string
  std::string str2 = ">" + str1;
  b.AddString(str2.c_str() + 1);

  EXPECT_EQ(a.ComputeHash(), b.ComputeHash());
}

}

