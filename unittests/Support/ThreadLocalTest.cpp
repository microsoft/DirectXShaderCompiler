//===- llvm/unittest/Support/ThreadLocalTest.cpp - ThreadLocal tests ------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ThreadLocalTest.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/ThreadLocal.h"
#include "gtest/gtest.h"
#include <type_traits>

using namespace llvm;
using namespace sys;

namespace {

class ThreadLocalTest : public ::testing::Test {
};

struct S {
  int i;
};

TEST_F(ThreadLocalTest, Basics) {
  ThreadLocal<const S> x;

  static_assert(
      std::is_const<std::remove_pointer<decltype(x.get())>::type>::value,
      "ThreadLocal::get didn't return a pointer to const object");

  EXPECT_EQ(nullptr, x.get());

  S s;
  x.set(&s);
  EXPECT_EQ(&s, x.get());

  x.erase();
  EXPECT_EQ(nullptr, x.get());

  ThreadLocal<S> y;

  static_assert(
      !std::is_const<std::remove_pointer<decltype(y.get())>::type>::value,
      "ThreadLocal::get returned a pointer to const object");

  EXPECT_EQ(nullptr, y.get());

  y.set(&s);
  EXPECT_EQ(&s, y.get());

  y.erase();
  EXPECT_EQ(nullptr, y.get());
}

}
