//===- unittests/SPIRV/ConstantTest.cpp ---------- Constant tests ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SPIRVTestUtils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/SPIRVContext.h"

using namespace clang::spirv;

namespace {
using ::testing::ContainerEq;
using ::testing::ElementsAre;

TEST(Constant, True) {
  SPIRVContext ctx;
  const Constant *c = Constant::getTrue(ctx, 2);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpConstantTrue, {2, 3});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, False) {
  SPIRVContext ctx;
  const Constant *c = Constant::getFalse(ctx, 2);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpConstantFalse, {2, 3});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Uint32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getUint32(ctx, 2, 7u);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpConstant, {2, 3, 7u});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Int32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getInt32(ctx, 2, -7);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpConstant, {2, 3, 0xFFFFFFF9});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Float32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getFloat32(ctx, 2, 7.0);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(
      spv::Op::OpConstant, {2, 3, cast::BitwiseCast<uint32_t, float>(7.0)});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Composite) {
  SPIRVContext ctx;
  const Constant *c = Constant::getComposite(ctx, 8, {4, 5, 6, 7});
  const auto result = c->withResultId(9);
  const auto expected =
      constructInst(spv::Op::OpConstantComposite, {8, 9, 4, 5, 6, 7});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Sampler) {
  SPIRVContext ctx;
  const Constant *c =
      Constant::getSampler(ctx, 8, spv::SamplerAddressingMode::Repeat, 1,
                           spv::SamplerFilterMode::Linear);
  const auto result = c->withResultId(9);
  const auto expected = constructInst(
      spv::Op::OpConstantSampler,
      {8, 9, static_cast<uint32_t>(spv::SamplerAddressingMode::Repeat), 1,
       static_cast<uint32_t>(spv::SamplerFilterMode::Linear)});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, Null) {
  SPIRVContext ctx;
  const Constant *c = Constant::getNull(ctx, 8);
  const auto result = c->withResultId(9);
  const auto expected = constructInst(spv::Op::OpConstantNull, {8, 9});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(Constant, ConstantsWithSameBitPatternButDifferentTypeIdAreNotEqual) {
  SPIRVContext ctx;

  const Constant *int1 = Constant::getInt32(ctx, /*type_id*/ 1, 0);
  const Constant *uint1 = Constant::getUint32(ctx, /*type_id*/ 2, 0);
  const Constant *float1 = Constant::getFloat32(ctx, /*type_id*/ 3, 0);
  const Constant *anotherInt1 = Constant::getInt32(ctx, /*type_id*/ 4, 0);

  EXPECT_FALSE(*int1 == *uint1);
  EXPECT_FALSE(*int1 == *float1);
  EXPECT_FALSE(*uint1 == *float1);
  EXPECT_FALSE(*int1 == *anotherInt1);
}

} // anonymous namespace
