//===- unittests/SPIRV/SpirvConstantTest.cpp --- SPIR-V Constant tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "clang/SPIRV/SpirvInstruction.h"

namespace {
using namespace clang::spirv;

TEST(SpirvConstant, Float16) {
  const uint16_t f16 = 12;
  SpirvConstantFloat constant(f16, {}, 0, {});
  EXPECT_EQ(f16, constant.getValue16());
}

TEST(SpirvConstant, Float32) {
  const float f32 = 1.5;
  SpirvConstantFloat constant(f32, {}, 0, {});
  EXPECT_EQ(f32, constant.getValue32());
}

TEST(SpirvConstant, Float64) {
  const double f64 = 3.14;
  SpirvConstantFloat constant(f64, {}, 0, {});
  EXPECT_EQ(f64, constant.getValue64());
}

} // anonymous namespace
