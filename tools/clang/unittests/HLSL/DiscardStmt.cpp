///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DiscardStmt.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "CompilationResult.h"
#include "HLSLTestData.h"

#include "WexTestClass.h"
#include "HlslTestUtils.h"

class DiscardTestFixture {
public:
  BEGIN_TEST_CLASS(DiscardTestFixture)
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()
  TEST_METHOD(TestBasic);
};

TEST_F(DiscardTestFixture, TestBasic) {
  CompilationResult result(CompilationResult::CreateForProgram("void foo() { discard; }", 0));
  EXPECT_EQ(true, result.ParseSucceeded());
}
