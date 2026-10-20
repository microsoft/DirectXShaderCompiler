// Copyright (c) Microsoft Corporation. All rights reserved.
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

// Exercise the first inclusion with the macro defined by Microsoft's pdb.h.
#define pure = 0
#include "llvm/Support/Compiler.h"

struct CompilerPureMacroTest {
  virtual ~CompilerPureMacroTest() = default;
  virtual void Method() pure;
};

LLVM_READONLY static int CompilerReadOnlyValue() { return 0; }

#undef pure

#include "gtest/gtest.h"
#include <type_traits>

TEST(CompilerTest, PureMacroCollision) {
  EXPECT_TRUE(std::is_abstract<CompilerPureMacroTest>::value);
  EXPECT_EQ(0, CompilerReadOnlyValue());
}
