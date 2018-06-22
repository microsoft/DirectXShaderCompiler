//===- unittests/SPIRV/DecorationTest.cpp ----- Decoration tests ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SPIRVTestUtils.h"
#include "gmock/gmock.h"
#include "clang/SPIRV/Decoration.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/String.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {
using ::testing::ElementsAre;
using ::testing::ContainerEq;

TEST(Decoration, SameDecorationWoParameterShouldHaveSameAddress) {
  SPIRVContext ctx;
  const Decoration *relaxed = Decoration::getRelaxedPrecision(ctx);
  const Decoration *relaxed_2 = Decoration::getRelaxedPrecision(ctx);
  EXPECT_EQ(relaxed, relaxed_2);
}

TEST(Decoration, SameMemberDecorationWoParameterShouldHaveSameAddress) {
  SPIRVContext ctx;
  const Decoration *mem_0_builtin_pos =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);
  const Decoration *mem_0_builtin_pos_again =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);

  // We must get the same pointer for the same decoration signature.
  EXPECT_EQ(mem_0_builtin_pos, mem_0_builtin_pos_again);
}

TEST(Decoration, DifferentMemberIndexDecorationShouldHaveDifferentAddress) {
  SPIRVContext ctx;
  // Applied to member 0 of a structure.
  const Decoration *mem_0_builtin_pos =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);
  // Applied to member 1 of a structure.
  const Decoration *mem_1_builtin_pos =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 1);

  // We should get different pointers for these decorations as they are
  // applied to different structure members.
  EXPECT_NE(mem_0_builtin_pos, mem_1_builtin_pos);
}

TEST(Decoration, RelaxedPrecision) {
  SPIRVContext ctx;
  const Decoration *rp = Decoration::getRelaxedPrecision(ctx);
  EXPECT_EQ(rp->getValue(), spv::Decoration::RelaxedPrecision);
  EXPECT_FALSE(rp->getMemberIndex().hasValue());
  EXPECT_TRUE(rp->getArgs().empty());
}

TEST(Decoration, SpecId) {
  SPIRVContext ctx;
  const Decoration *specId = Decoration::getSpecId(ctx, 15);
  EXPECT_EQ(specId->getValue(), spv::Decoration::SpecId);
  EXPECT_FALSE(specId->getMemberIndex().hasValue());
  EXPECT_THAT(specId->getArgs(), ElementsAre(15));
}

TEST(Decoration, Block) {
  SPIRVContext ctx;
  const Decoration *block = Decoration::getBlock(ctx);
  EXPECT_EQ(block->getValue(), spv::Decoration::Block);
  EXPECT_FALSE(block->getMemberIndex().hasValue());
  EXPECT_TRUE(block->getArgs().empty());
}

TEST(Decoration, BufferBlock) {
  SPIRVContext ctx;
  const Decoration *bufferblock = Decoration::getBufferBlock(ctx);
  EXPECT_EQ(bufferblock->getValue(), spv::Decoration::BufferBlock);
  EXPECT_FALSE(bufferblock->getMemberIndex().hasValue());
  EXPECT_TRUE(bufferblock->getArgs().empty());
}

TEST(Decoration, RowMajor) {
  SPIRVContext ctx;
  const Decoration *rowMajor = Decoration::getRowMajor(ctx, 2);
  EXPECT_EQ(rowMajor->getValue(), spv::Decoration::RowMajor);
  EXPECT_EQ(rowMajor->getMemberIndex().getValue(), 2U);
  EXPECT_TRUE(rowMajor->getArgs().empty());
}

TEST(Decoration, ColMajor) {
  SPIRVContext ctx;
  const Decoration *colMajor = Decoration::getColMajor(ctx, 2);
  EXPECT_EQ(colMajor->getValue(), spv::Decoration::ColMajor);
  EXPECT_EQ(colMajor->getMemberIndex().getValue(), 2U);
  EXPECT_TRUE(colMajor->getArgs().empty());
}

TEST(Decoration, ArrayStride) {
  SPIRVContext ctx;
  const Decoration *arrStride = Decoration::getArrayStride(ctx, 4);
  EXPECT_EQ(arrStride->getValue(), spv::Decoration::ArrayStride);
  EXPECT_FALSE(arrStride->getMemberIndex().hasValue());
  EXPECT_THAT(arrStride->getArgs(), ElementsAre(4));
}

TEST(Decoration, MatrixStride) {
  SPIRVContext ctx;
  const Decoration *matStride = Decoration::getMatrixStride(ctx, 4, 7);
  EXPECT_EQ(matStride->getValue(), spv::Decoration::MatrixStride);
  EXPECT_EQ(matStride->getMemberIndex().getValue(), 7U);
  EXPECT_THAT(matStride->getArgs(), ElementsAre(4));
}

TEST(Decoration, GLSLShared) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getGLSLShared(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::GLSLShared);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, GLSLPacked) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getGLSLPacked(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::GLSLPacked);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, CPacked) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getCPacked(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::CPacked);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, BuiltIn) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::InvocationId);
  EXPECT_EQ(dec->getValue(), spv::Decoration::BuiltIn);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(),
              ElementsAre(static_cast<uint32_t>(spv::BuiltIn::InvocationId)));
}

TEST(Decoration, BuiltInAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::BuiltIn);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(),
              ElementsAre(static_cast<uint32_t>(spv::BuiltIn::Position)));
}

TEST(Decoration, NoPerspective) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNoPerspective(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NoPerspective);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, NoPerspectiveAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNoPerspective(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NoPerspective);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}
TEST(Decoration, Flat) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getFlat(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Flat);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, FlatAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getFlat(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Flat);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Patch) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getPatch(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Patch);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, PatchAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getPatch(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Patch);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}
TEST(Decoration, Centroid) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getCentroid(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Centroid);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, CentroidAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getCentroid(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Centroid);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Sample) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getSample(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Sample);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, SampleAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getSample(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Sample);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Invariant) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getInvariant(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Invariant);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Restrict) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getRestrict(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Restrict);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Aliased) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getAliased(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Aliased);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Volatile) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getVolatile(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Volatile);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, VolatileAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getVolatile(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Volatile);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Constant) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getConstant(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Constant);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Coherent) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getCoherent(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Coherent);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, CoherentAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getCoherent(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Coherent);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}
TEST(Decoration, NonWritable) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNonWritable(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NonWritable);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, NonWritableAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNonWritable(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NonWritable);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, NonReadable) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNonReadable(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NonReadable);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, NonReadableAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNonReadable(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NonReadable);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Uniform) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getUniform(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Uniform);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, UniformAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getUniform(ctx, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Uniform);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, SaturatedConversion) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getSaturatedConversion(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::SaturatedConversion);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, Stream) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getStream(ctx, 7);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Stream);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, StreamAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getStream(ctx, 7, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Stream);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, Location) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getLocation(ctx, 7);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Location);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, LocationAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getLocation(ctx, 7, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Location);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, Component) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getComponent(ctx, 7);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Component);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, ComponentAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getComponent(ctx, 7, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Component);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, Index) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getIndex(ctx, 2);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Index);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(2));
}

TEST(Decoration, Binding) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getBinding(ctx, 2);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Binding);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(2));
}

TEST(Decoration, DescriptorSet) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getDescriptorSet(ctx, 1);
  EXPECT_EQ(dec->getValue(), spv::Decoration::DescriptorSet);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(1));
}

TEST(Decoration, Offset) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getOffset(ctx, 3, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Offset);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(3));
}

TEST(Decoration, XfbBuffer) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getXfbBuffer(ctx, 7);
  EXPECT_EQ(dec->getValue(), spv::Decoration::XfbBuffer);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, XfbBufferAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getXfbBuffer(ctx, 7, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::XfbBuffer);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, XfbStride) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getXfbStride(ctx, 7);
  EXPECT_EQ(dec->getValue(), spv::Decoration::XfbStride);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, XfbStrideAppliedToMember) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getXfbStride(ctx, 7, 3);
  EXPECT_EQ(dec->getValue(), spv::Decoration::XfbStride);
  EXPECT_EQ(dec->getMemberIndex().getValue(), 3U);
  EXPECT_THAT(dec->getArgs(), ElementsAre(7));
}

TEST(Decoration, FuncParamAttr) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getFuncParamAttr(ctx, spv::FunctionParameterAttribute::Zext);
  EXPECT_EQ(dec->getValue(), spv::Decoration::FuncParamAttr);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(static_cast<uint32_t>(
                                  spv::FunctionParameterAttribute::Zext)));
}

TEST(Decoration, FPRoundingMode) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getFPRoundingMode(ctx, spv::FPRoundingMode::RTZ);
  EXPECT_EQ(dec->getValue(), spv::Decoration::FPRoundingMode);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(),
              ElementsAre(static_cast<uint32_t>(spv::FPRoundingMode::RTZ)));
}

TEST(Decoration, FPFastMathMode) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getFPFastMathMode(ctx, spv::FPFastMathModeShift::NotInf);
  EXPECT_EQ(dec->getValue(), spv::Decoration::FPFastMathMode);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(static_cast<uint32_t>(
                                  spv::FPFastMathModeShift::NotInf)));
}

TEST(Decoration, LinkageAttributes) {
  SPIRVContext ctx;
  const Decoration *dec =
      Decoration::getLinkageAttributes(ctx, "main", spv::LinkageType::Export);
  EXPECT_EQ(dec->getValue(), spv::Decoration::LinkageAttributes);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());

  // Check arguments are as expected
  std::vector<uint32_t> args =
      std::vector<uint32_t>(dec->getArgs().begin(), dec->getArgs().end());
  std::vector<uint32_t> expectedArgs = string::encodeSPIRVString("main");
  expectedArgs.push_back(static_cast<uint32_t>(spv::LinkageType::Export));
  EXPECT_EQ(args, expectedArgs);
}

TEST(Decoration, NoContraction) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getNoContraction(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::NoContraction);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, InputAttachmentIndex) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getInputAttachmentIndex(ctx, 1);
  EXPECT_EQ(dec->getValue(), spv::Decoration::InputAttachmentIndex);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(1));
}

TEST(Decoration, Alignment) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getAlignment(ctx, 1);
  EXPECT_EQ(dec->getValue(), spv::Decoration::Alignment);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_THAT(dec->getArgs(), ElementsAre(1));
}

TEST(Decoration, OverrideCoverageNV) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getOverrideCoverageNV(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::OverrideCoverageNV);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, PassthroughNV) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getPassthroughNV(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::PassthroughNV);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, ViewportRelativeNV) {
  SPIRVContext ctx;
  const Decoration *dec = Decoration::getViewportRelativeNV(ctx);
  EXPECT_EQ(dec->getValue(), spv::Decoration::ViewportRelativeNV);
  EXPECT_FALSE(dec->getMemberIndex().hasValue());
  EXPECT_TRUE(dec->getArgs().empty());
}

TEST(Decoration, BlockWithTargetId) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getBlock(ctx);
  const auto result = d->withTargetId(1);
  const auto expected = constructInst(
      spv::Op::OpDecorate, {1, static_cast<uint32_t>(spv::Decoration::Block)});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(Decoration, RowMajorWithTargetId) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getRowMajor(ctx, 3);
  const auto result = d->withTargetId(2);
  const auto expected =
      constructInst(spv::Op::OpMemberDecorate,
                    {2, 3, static_cast<uint32_t>(spv::Decoration::RowMajor)});
  EXPECT_THAT(result, ContainerEq(expected));
}

} // anonymous namespace
