//===- unittests/SPIRV/StructureTest.cpp ------ SPIR-V structures tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Structure.h"

#include "SPIRVTestUtils.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;

TEST(Structure, DefaultConstructedBasicBlockIsEmpty) {
  auto bb = BasicBlock();
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, TakeBasicBlockHaveAllContents) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);

  auto bb = BasicBlock(42);
  bb.addInstruction(constructInst(spv::Op::OpReturn, {}));
  bb.take(&ib);

  std::vector<uint32_t> expected;
  appendVector(&expected, constructInst(spv::Op::OpLabel, {42}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, AfterClearBasicBlockIsEmpty) {
  auto bb = BasicBlock(42);
  bb.addInstruction(constructInst(spv::Op::OpNop, {}));
  EXPECT_FALSE(bb.isEmpty());
  bb.clear();
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, DefaultConstructedFunctionIsEmpty) {
  auto f = Function();
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, TakeFunctionHaveAllContents) {
  auto f = Function(1, 2, spv::FunctionControlMask::Inline, 3);
  f.addParameter(1, 42);

  auto bb = BasicBlock(10);
  bb.addInstruction(constructInst(spv::Op::OpReturn, {}));
  f.addBasicBlock(std::move(bb));

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  f.take(&ib);

  std::vector<uint32_t> expected;
  appendVector(&expected, constructInst(spv::Op::OpFunction, {1, 2, 1, 3}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionParameter, {1, 42}));
  appendVector(&expected, constructInst(spv::Op::OpLabel, {10}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, AfterClearFunctionIsEmpty) {
  auto f = Function(1, 2, spv::FunctionControlMask::Inline, 3);
  f.addParameter(1, 42);
  EXPECT_FALSE(f.isEmpty());
  f.clear();
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, DefaultConstructedModuleIsEmpty) {
  auto m = SPIRVModule();
  EXPECT_TRUE(m.isEmpty());
}

TEST(Structure, AfterClearModuleIsEmpty) {
  auto m = SPIRVModule();
  m.setBound(12);
  EXPECT_FALSE(m.isEmpty());
  m.clear();
  EXPECT_TRUE(m.isEmpty());
}

TEST(Structure, TakeModuleHaveAllContents) {
  auto m = SPIRVModule();
  std::vector<uint32_t> expected{spv::MagicNumber, spv::Version,
                                 /* generator */ 14u << 16, /* bound */ 6, 0};

  m.addCapability(spv::Capability::Shader);
  appendVector(&expected,
               constructInst(spv::Op::OpCapability,
                             {static_cast<uint32_t>(spv::Capability::Shader)}));

  m.addExtension("ext");
  const uint32_t extWord = 'e' | ('x' << 8) | ('t' << 16);
  appendVector(&expected, constructInst(spv::Op::OpExtension, {extWord}));

  m.addExtInstSet(5, "gl");
  const uint32_t glWord = 'g' | ('l' << 8);
  appendVector(&expected, constructInst(spv::Op::OpExtInstImport, {5, glWord}));

  m.setAddressingModel(spv::AddressingModel::Logical);
  m.setMemoryModel(spv::MemoryModel::GLSL450);
  appendVector(
      &expected,
      constructInst(spv::Op::OpMemoryModel,
                    {static_cast<uint32_t>(spv::AddressingModel::Logical),
                     static_cast<uint32_t>(spv::MemoryModel::GLSL450)}));

  m.addEntryPoint(spv::ExecutionModel::Fragment, 2, "main", {42});
  const uint32_t mainWord = 'm' | ('a' << 8) | ('i' << 16) | ('n' << 24);
  appendVector(
      &expected,
      constructInst(spv::Op::OpEntryPoint,
                    {static_cast<uint32_t>(spv::ExecutionModel::Fragment), 2,
                     mainWord, /* addtional null in name */ 0, 42}));

  m.addExecutionMode(constructInst(
      spv::Op::OpExecutionMode,
      {2, static_cast<uint32_t>(spv::ExecutionMode::OriginUpperLeft)}));
  appendVector(&expected,
               constructInst(spv::Op::OpExecutionMode,
                             {2, static_cast<uint32_t>(
                                     spv::ExecutionMode::OriginUpperLeft)}));

  // TODO: other debug instructions

  m.addDebugName(2, llvm::None, "main");
  appendVector(&expected,
               constructInst(spv::Op::OpName,
                             {2, mainWord, /* additional null in name */ 0}));

  m.addDecoration(constructInst(
      spv::Op::OpDecorate,
      {2, static_cast<uint32_t>(spv::Decoration::RelaxedPrecision)}));
  appendVector(&expected,
               constructInst(spv::Op::OpDecorate,
                             {2, static_cast<uint32_t>(
                                     spv::Decoration::RelaxedPrecision)}));

  m.addType(constructInst(spv::Op::OpTypeVoid, {1}));
  appendVector(&expected, constructInst(spv::Op::OpTypeVoid, {1}));

  m.addType(constructInst(spv::Op::OpTypeFunction, {3, 1, 1}));
  appendVector(&expected, constructInst(spv::Op::OpTypeFunction, {3, 1, 1}));

  // TODO: constant
  // TODO: variable

  auto f = Function(1, 2, spv::FunctionControlMask::MaskNone, 3);
  auto bb = BasicBlock(4);
  bb.addInstruction(constructInst(spv::Op::OpReturn, {}));
  f.addBasicBlock(std::move(bb));
  m.addFunction(std::move(f));
  appendVector(&expected, constructInst(spv::Op::OpFunction, {1, 2, 0, 3}));
  appendVector(&expected, constructInst(spv::Op::OpLabel, {4}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));

  m.setBound(6);

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  m.take(&ib);

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(m.isEmpty());
}

} // anonymous namespace