//===- unittests/SPIRV/InstBuilderTest.cpp ------ InstBuilder tests -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/InstBuilder.h"

#include "SPIRVTestUtils.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;

TEST(InstBuilder, InstWoParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  expectBuildSuccess(ib.opNop().x());
  // OpNop takes no parameters.
  auto expected = constructInst(spv::Op::OpNop, {});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(InstBuilder, InstWFixedNumberOfParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // OpMemoryModel takes a fixed number of parameters.
  expectBuildSuccess(
      ib.opMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450)
          .x());
  auto expected =
      constructInst(spv::Op::OpMemoryModel,
                    {static_cast<uint32_t>(spv::AddressingModel::Logical),
                     static_cast<uint32_t>(spv::MemoryModel::GLSL450)});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(InstBuilder, InstWAdditionalParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // OpDecorate <target-id> ArrayStride needs an additional literal number.
  expectBuildSuccess(
      ib.opDecorate(1, spv::Decoration::ArrayStride).literalInteger(42).x());
  auto expected = constructInst(
      spv::Op::OpDecorate,
      {1, static_cast<uint32_t>(spv::Decoration::ArrayStride), 42});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(InstBuilder, InstWBitEnumParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  auto control =
      spv::LoopControlMask::Unroll | spv::LoopControlMask::DontUnroll;
  // OpLoopMerge takes an BitEnum parameter.
  expectBuildSuccess(ib.opLoopMerge(1, 2, control).x());
  auto expected = constructInst(spv::Op::OpLoopMerge,
                                {1, 2, static_cast<uint32_t>(control)});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(InstBuilder, InstWBitEnumParamsNeedAdditionalParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Aligned requires an additional literal integer.
  auto access =
      spv::MemoryAccessMask::Nontemporal | spv::MemoryAccessMask::Aligned;
  expectBuildSuccess(
      ib.opStore(1, 2, llvm::Optional<spv::MemoryAccessMask>(access))
          .literalInteger(16)
          .x());
  auto expected = constructInst(spv::Op::OpStore,
                                {1, 2, static_cast<uint32_t>(access), 16});
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(InstBuilder, InstMissingAdditionalBuiltin) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the builtIn parameter required by Builtin decoration causes an
  // error.
  EXPECT_EQ(InstBuilder::Status::ExpectBuiltIn,
            ib.opDecorate(1, spv::Decoration::BuiltIn).x());
}

TEST(InstBuilder, InstMissingAdditionalFPFastMathMode) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the FPFastMathMode parameter required by FPFastMathMode decoration
  // causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectFPFastMathMode,
            ib.opDecorate(1, spv::Decoration::FPFastMathMode).x());
}

TEST(InstBuilder, InstMissingAdditionalFPRoundingMode) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the FPRoundingMode parameter required by RoundingMode decoration
  // causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectFPRoundingMode,
            ib.opDecorate(1, spv::Decoration::FPRoundingMode).x());
}

TEST(InstBuilder, InstMissingAdditionalFuncParamAttr) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the FunctionParameterAttribute parameter required by
  // FuncParamAttr decoration causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectFunctionParameterAttribute,
            ib.opDecorate(1, spv::Decoration::FuncParamAttr).x());
}

TEST(InstBuilder, InstMissingAdditionalIdRef) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the IdRef parameter required by ImageOperands causes an error.
  EXPECT_EQ(
      InstBuilder::Status::ExpectIdRef,
      ib.opImageSampleImplicitLod(1, 2, 3, 4, spv::ImageOperandsMask::Bias)
          .x());
}

TEST(InstBuilder, InstMissingAdditionalLinkageType) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the LinkageType parameter required by the LinkageAttributes
  // decoration causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectLinkageType,
            ib.opDecorate(1, spv::Decoration::LinkageAttributes)
                .literalString("name")
                .x());
}

TEST(InstBuilder, InstMissingAdditionalLiteralInteger) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the literal integer required by ArrayStride causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectLiteralInteger,
            ib.opDecorate(1, spv::Decoration::ArrayStride).x());
}

TEST(InstBuilder, InstMissingAdditionalLiteralString) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  // Missing the LiteralString parameter required by the LinkageAttributes
  // decoration causes an error.
  EXPECT_EQ(InstBuilder::Status::ExpectLiteralString,
            ib.opDecorate(1, spv::Decoration::LinkageAttributes).x());
}

TEST(InstBuilder, NullConsumerResultsInError) {
  auto ib = InstBuilder(nullptr);
  EXPECT_EQ(InstBuilder::Status::NullConsumer, ib.opNop().x());
}

TEST(InstBuilder, InstWStringParams) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);

  expectBuildSuccess(ib.opString(1, "").x());
  expectBuildSuccess(ib.opString(2, "m").x());
  expectBuildSuccess(ib.opString(3, "ma").x());
  expectBuildSuccess(ib.opString(4, "mai").x());
  expectBuildSuccess(ib.opString(5, "main").x());
  expectBuildSuccess(ib.opString(6, "mainf").x());

  SimpleInstBuilder sib;
  uint32_t strWord = 0;
  sib.inst(spv::Op::OpString, {1, strWord});
  strWord = 'm';
  sib.inst(spv::Op::OpString, {2, strWord});
  strWord |= 'a' << 8;
  sib.inst(spv::Op::OpString, {3, strWord});
  strWord |= 'i' << 16;
  sib.inst(spv::Op::OpString, {4, strWord});
  strWord |= 'n' << 24;
  sib.inst(spv::Op::OpString, {5, strWord, 0});
  sib.inst(spv::Op::OpString, {6, strWord, 'f'});

  EXPECT_THAT(result, ContainerEq(sib.get()));
}
// TOOD: Add tests for providing more parameters than needed

} // anonymous namespace
