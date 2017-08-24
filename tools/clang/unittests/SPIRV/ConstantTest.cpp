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
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {
using ::testing::ElementsAre;
using ::testing::ContainerEq;

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
TEST(Constant, SpecTrue) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecTrue(ctx, 2);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpSpecConstantTrue, {2, 3});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, SpecFalse) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecFalse(ctx, 2);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpSpecConstantFalse, {2, 3});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, SpecUint32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecUint32(ctx, 2, 7u);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(spv::Op::OpSpecConstant, {2, 3, 7u});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, SpecInt32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecInt32(ctx, 2, -7);
  const auto result = c->withResultId(3);
  const auto expected =
      constructInst(spv::Op::OpSpecConstant, {2, 3, 0xFFFFFFF9});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, SpecFloat32) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecFloat32(ctx, 2, 7.0);
  const auto result = c->withResultId(3);
  const auto expected = constructInst(
      spv::Op::OpSpecConstant, {2, 3, cast::BitwiseCast<uint32_t, float>(7.0)});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, SpecComposite) {
  SPIRVContext ctx;
  const Constant *c = Constant::getSpecComposite(ctx, 8, {4, 5, 6, 7});
  const auto result = c->withResultId(9);
  const auto expected =
      constructInst(spv::Op::OpSpecConstantComposite, {8, 9, 4, 5, 6, 7});
  EXPECT_THAT(result, ContainerEq(expected));
}
TEST(Constant, DecoratedTrue) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getTrue(ctx, 2, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstantTrue);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_TRUE(c->getArgs().empty());
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedFalse) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getFalse(ctx, 2, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstantFalse);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_TRUE(c->getArgs().empty());
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedUint32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getUint32(ctx, 2, 7u, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(), ElementsAre(7u));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedInt32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getInt32(ctx, 2, -7, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(), ElementsAre(0xFFFFFFF9));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedFloat32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getFloat32(ctx, 2, 7.0f, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(),
              ElementsAre(cast::BitwiseCast<uint32_t, float>(7.0)));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedComposite) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getComposite(ctx, 8, {4, 5, 6, 7}, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstantComposite);
  EXPECT_EQ(c->getTypeId(), 8);
  EXPECT_THAT(c->getArgs(), ElementsAre(4, 5, 6, 7));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSampler) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c =
      Constant::getSampler(ctx, 8, spv::SamplerAddressingMode::Repeat, 1,
                           spv::SamplerFilterMode::Linear, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstantSampler);
  EXPECT_EQ(c->getTypeId(), 8);
  EXPECT_THAT(
      c->getArgs(),
      ElementsAre(static_cast<uint32_t>(spv::SamplerAddressingMode::Repeat), 1,
                  static_cast<uint32_t>(spv::SamplerFilterMode::Linear)));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedNull) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getNull(ctx, 2, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpConstantNull);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_TRUE(c->getArgs().empty());
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecTrue) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecTrue(ctx, 2, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstantTrue);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_TRUE(c->getArgs().empty());
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecFalse) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecFalse(ctx, 2, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstantFalse);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_TRUE(c->getArgs().empty());
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecUint32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecUint32(ctx, 2, 7u, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(), ElementsAre(7u));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecInt32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecInt32(ctx, 2, -7, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(), ElementsAre(0xFFFFFFF9));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecFloat32) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecFloat32(ctx, 2, 7.0f, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstant);
  EXPECT_EQ(c->getTypeId(), 2);
  EXPECT_THAT(c->getArgs(),
              ElementsAre(cast::BitwiseCast<uint32_t, float>(7.0)));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
}
TEST(Constant, DecoratedSpecComposite) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getSpecId(ctx, 5);
  const Constant *c = Constant::getSpecComposite(ctx, 8, {4, 5, 6, 7}, {d});
  EXPECT_EQ(c->getOpcode(), spv::Op::OpSpecConstantComposite);
  EXPECT_EQ(c->getTypeId(), 8);
  EXPECT_THAT(c->getArgs(), ElementsAre(4, 5, 6, 7));
  EXPECT_THAT(c->getDecorations(), ElementsAre(d));
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