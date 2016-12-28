//===- llvm/unittest/ADT/MakeUniqueTest.cpp - make_unique unit tests ------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FunctionRefTest.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/STLExtras.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

// Ensure that copies of a function_ref copy the underlying state rather than
// causing one function_ref to chain to the next.
TEST(FunctionRefTest, Copy) {
  auto A = [] { return 1; };
  auto B = [] { return 2; };
  function_ref<int()> X = A;
  function_ref<int()> Y = X;
  X = B;
  EXPECT_EQ(1, Y());
}

}
