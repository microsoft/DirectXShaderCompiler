//===- unittests/SPIRV/ModuleBuilderTest.cpp ------ ModuleBuilder tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/ModuleBuilder.h"
#include "spirv/1.0/spirv.hpp11"

#include "SPIRVTestUtils.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;
using ::testing::ElementsAre;

void expectBuildSuccess(ModuleBuilder::Status status) {
  EXPECT_EQ(ModuleBuilder::Status::Success, status);
}

TEST(ModuleBuilder, BeginAndThenEndModuleCreatesHeader) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.endModule());
  std::vector<uint32_t> spvModule = builder.takeModule();

  // At the very least, running BeginModule() and EndModule() should
  // create the SPIR-V Header. The header is exactly 5 words long.
  EXPECT_EQ(spvModule.size(), 5u);
  EXPECT_THAT(spvModule,
              ElementsAre(spv::MagicNumber, spv::Version, 14u << 16, 1u, 0u));
}

TEST(ModuleBuilder, BeginEndFunctionCreatesFunction) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  const auto rType = context.takeNextId();
  const auto fType = context.takeNextId();
  const auto fId = context.getNextId();
  expectBuildSuccess(builder.beginFunction(fType, rType));
  expectBuildSuccess(builder.endFunction());
  expectBuildSuccess(builder.endModule());
  const auto result = builder.takeModule();

  auto expected = getModuleHeader(context.getNextId());
  appendVector(&expected,
               constructInst(spv::Op::OpFunction, {rType, fId, 0, fType}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));
  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(ModuleBuilder, BeginEndBasicBlockCreatesBasicBlock) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  const auto rType = context.takeNextId();
  const auto fType = context.takeNextId();
  const auto fId = context.getNextId();
  expectBuildSuccess(builder.beginFunction(fType, rType));
  const auto labelId = context.getNextId();
  expectBuildSuccess(builder.beginBasicBlock());
  expectBuildSuccess(builder.endBasicBlockWithReturn());
  expectBuildSuccess(builder.endFunction());
  expectBuildSuccess(builder.endModule());
  const auto result = builder.takeModule();

  auto expected = getModuleHeader(context.getNextId());
  appendVector(&expected,
               constructInst(spv::Op::OpFunction, {rType, fId, 0, fType}));
  appendVector(&expected, constructInst(spv::Op::OpLabel, {labelId}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));

  EXPECT_THAT(result, ContainerEq(expected));
}

TEST(ModuleBuilder, NestedModuleResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginFunction(1, 2));
  EXPECT_EQ(ModuleBuilder::Status::ErrNestedModule, builder.beginModule());
}

TEST(ModuleBuilder, NestedFunctionResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginFunction(1, 2));
  EXPECT_EQ(ModuleBuilder::Status::ErrNestedFunction,
            builder.beginFunction(3, 4));
}

TEST(ModuleBuilder, NestedBasicBlockResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginFunction(1, 2));
  expectBuildSuccess(builder.beginBasicBlock());
  EXPECT_EQ(ModuleBuilder::Status::ErrNestedBasicBlock,
            builder.beginBasicBlock());
}

TEST(ModuleBuilder, BasicBlockWoFunctionResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  EXPECT_EQ(ModuleBuilder::Status::ErrDetachedBasicBlock,
            builder.beginBasicBlock());
}

TEST(ModuleBuilder, EndFunctionWoBeginFunctionResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  EXPECT_EQ(ModuleBuilder::Status::ErrNoActiveFunction, builder.endFunction());
}

TEST(ModuleBuilder, EndFunctionWActiveBasicBlockResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginFunction(1, 2));
  expectBuildSuccess(builder.beginBasicBlock());
  EXPECT_EQ(ModuleBuilder::Status::ErrActiveBasicBlock, builder.endFunction());
}

TEST(ModuleBuilder, ReturnWActiveBasicBlockResultsInError) {
  SPIRVContext context;
  ModuleBuilder builder(&context);

  expectBuildSuccess(builder.beginModule());
  expectBuildSuccess(builder.beginFunction(1, 2));
  EXPECT_EQ(ModuleBuilder::Status::ErrNoActiveBasicBlock,
            builder.endBasicBlockWithReturn());
}

} // anonymous namespace
