//===- unittests/SPIRV/ModuleBuilderTest.cpp ------ ModuleBuilder tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/ModuleBuilder.h"
#include "spirv/unified1/spirv.hpp11"

#include "SPIRVTestUtils.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;
using ::testing::ElementsAre;

TEST(ModuleBuilder, TakeModuleDirectlyCreatesHeader) {
  SPIRVContext context;
  ModuleBuilder builder(&context, nullptr, {});

  EXPECT_THAT(builder.takeModule(),
              ElementsAre(spv::MagicNumber, 0x00010000, 14u << 16, 1u, 0u));
}

TEST(ModuleBuilder, CreateFunction) {
  SPIRVContext context;
  ModuleBuilder builder(&context, nullptr, {});

  const auto rType = context.takeNextId();
  const auto fType = context.takeNextId();
  const auto fId = context.getNextId();
  const auto resultId = builder.beginFunction(fType, rType);
  EXPECT_EQ(fId, resultId);
  EXPECT_TRUE(builder.endFunction());
  const auto result = builder.takeModule();

  SimpleInstBuilder sib(context.getNextId());
  sib.inst(spv::Op::OpFunction, {rType, fId, 0, fType});
  sib.inst(spv::Op::OpFunctionEnd, {});
  EXPECT_THAT(result, ContainerEq(sib.get()));
}

TEST(ModuleBuilder, CreateBasicBlock) {
  SPIRVContext context;
  ModuleBuilder builder(&context, nullptr, {});

  const auto rType = context.takeNextId();
  const auto fType = context.takeNextId();
  const auto fId = context.getNextId();
  EXPECT_NE(0U, builder.beginFunction(fType, rType));
  const auto labelId = context.getNextId();
  const auto resultId = builder.createBasicBlock();
  EXPECT_EQ(labelId, resultId);
  builder.setInsertPoint(resultId);
  builder.createReturn();
  EXPECT_TRUE(builder.endFunction());

  const auto result = builder.takeModule();

  SimpleInstBuilder sib(context.getNextId());
  sib.inst(spv::Op::OpFunction, {rType, fId, 0, fType});
  sib.inst(spv::Op::OpLabel, {labelId});
  sib.inst(spv::Op::OpReturn, {});
  sib.inst(spv::Op::OpFunctionEnd, {});

  EXPECT_THAT(result, ContainerEq(sib.get()));
}

} // anonymous namespace
