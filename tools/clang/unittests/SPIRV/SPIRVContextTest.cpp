//===- unittests/SPIRV/SPIRVContextTest.cpp ----- SPIRVContext tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {

TEST(ValidateSPIRVContext, ValidateGetNextId) {
  SPIRVContext context;
  // Check that the first ID is 1.
  EXPECT_EQ(context.getNextId(), 1u);
  // Check that calling getNextId() multiple times does not increment the ID
  EXPECT_EQ(context.getNextId(), 1u);
}

TEST(ValidateSPIRVContext, ValidateTakeNextId) {
  SPIRVContext context;
  EXPECT_EQ(context.takeNextId(), 1u);
  EXPECT_EQ(context.takeNextId(), 2u);
  EXPECT_EQ(context.getNextId(), 3u);
}

// TODO: Add more SPIRVContext tests

} // anonymous namespace
