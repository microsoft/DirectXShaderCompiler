//===- llvm/unittest/Support/formatted_raw_ostream_test.cpp ---------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// formatted_raw_ostream_test.cpp                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/FormattedStream.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(formatted_raw_ostreamTest, Test_Tell) {
  // Check offset when underlying stream has buffer contents.
  SmallString<128> A;
  raw_svector_ostream B(A);
  formatted_raw_ostream C(B);
  char tmp[100] = "";

  for (unsigned i = 0; i != 3; ++i) {
    C.write(tmp, 100);

    EXPECT_EQ(100*(i+1), (unsigned) C.tell());
  }
}

}
