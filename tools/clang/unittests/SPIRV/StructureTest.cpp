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
#include "clang/SPIRV/SPIRVContext.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;

TEST(Structure, InstructionHasCorrectOpcode) {
  Instruction inst(constructInst(spv::Op::OpIAdd, {1, 2, 3, 4}));

  ASSERT_TRUE(!inst.isEmpty());
  EXPECT_EQ(inst.getOpcode(), spv::Op::OpIAdd);
}

TEST(Structure, InstructionGetOriginalContents) {
  Instruction inst(constructInst(spv::Op::OpIAdd, {1, 2, 3, 4}));

  EXPECT_THAT(inst.take(),
              ContainerEq(constructInst(spv::Op::OpIAdd, {1, 2, 3, 4})));
}

TEST(Structure, InstructionIsTerminator) {
  for (auto opcode :
       {spv::Op::OpKill, spv::Op::OpUnreachable, spv::Op::OpBranch,
        spv::Op::OpBranchConditional, spv::Op::OpSwitch, spv::Op::OpReturn,
        spv::Op::OpReturnValue}) {
    Instruction inst(constructInst(opcode, {/* wrong params here */ 1}));

    EXPECT_TRUE(inst.isTerminator());
  }
}

TEST(Structure, InstructionIsNotTerminator) {
  for (auto opcode : {spv::Op::OpNop, spv::Op::OpAccessChain, spv::Op::OpAll}) {
    Instruction inst(constructInst(opcode, {/* wrong params here */ 1}));

    EXPECT_FALSE(inst.isTerminator());
  }
}

TEST(Structure, TakeBasicBlockHaveAllContents) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);

  auto bb = BasicBlock(42);
  bb.appendInstruction(constructInst(spv::Op::OpReturn, {}));
  bb.take(&ib);

  SimpleInstBuilder sib;
  sib.inst(spv::Op::OpLabel, {42});
  sib.inst(spv::Op::OpReturn, {});

  EXPECT_THAT(result, ContainerEq(sib.get()));
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, AfterClearBasicBlockIsEmpty) {
  auto bb = BasicBlock(42);
  bb.appendInstruction(constructInst(spv::Op::OpNop, {}));
  EXPECT_FALSE(bb.isEmpty());
  bb.clear();
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, TakeFunctionHaveAllContents) {
  auto f = Function(1, 2, spv::FunctionControlMask::Inline, 3);
  f.addParameter(1, 42);

  auto bb = llvm::make_unique<BasicBlock>(10);
  bb->appendInstruction(constructInst(spv::Op::OpReturn, {}));
  f.addBasicBlock(std::move(bb));

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  f.take(&ib);

  SimpleInstBuilder sib;
  sib.inst(spv::Op::OpFunction, {1, 2, 1, 3});
  sib.inst(spv::Op::OpFunctionParameter, {1, 42});
  sib.inst(spv::Op::OpLabel, {10});
  sib.inst(spv::Op::OpReturn, {});
  sib.inst(spv::Op::OpFunctionEnd, {});

  EXPECT_THAT(result, ContainerEq(sib.get()));
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
  SPIRVContext context;
  auto m = SPIRVModule();

  // Will fix up the bound later.
  SimpleInstBuilder sib(0);

  m.addCapability(spv::Capability::Shader);
  sib.inst(spv::Op::OpCapability,
           {static_cast<uint32_t>(spv::Capability::Shader)});

  m.addExtension("ext");
  const uint32_t extWord = 'e' | ('x' << 8) | ('t' << 16);
  sib.inst(spv::Op::OpExtension, {extWord});

  const uint32_t extInstSetId = context.takeNextId();
  m.addExtInstSet(extInstSetId, "gl");
  const uint32_t glWord = 'g' | ('l' << 8);
  sib.inst(spv::Op::OpExtInstImport, {extInstSetId, glWord});

  m.setAddressingModel(spv::AddressingModel::Logical);
  m.setMemoryModel(spv::MemoryModel::GLSL450);
  sib.inst(spv::Op::OpMemoryModel,
           {static_cast<uint32_t>(spv::AddressingModel::Logical),
            static_cast<uint32_t>(spv::MemoryModel::GLSL450)});

  const uint32_t entryPointId = context.takeNextId();
  m.addEntryPoint(spv::ExecutionModel::Fragment, entryPointId, "main", {42});
  const uint32_t mainWord = 'm' | ('a' << 8) | ('i' << 16) | ('n' << 24);
  sib.inst(spv::Op::OpEntryPoint,
           {static_cast<uint32_t>(spv::ExecutionModel::Fragment), entryPointId,
            mainWord, /* addtional null in name */ 0, 42});

  m.addExecutionMode(constructInst(
      spv::Op::OpExecutionMode,
      {entryPointId,
       static_cast<uint32_t>(spv::ExecutionMode::OriginUpperLeft)}));
  sib.inst(spv::Op::OpExecutionMode,
           {entryPointId,
            static_cast<uint32_t>(spv::ExecutionMode::OriginUpperLeft)});

  // TODO: source code debug information

  m.addDebugName(entryPointId, "main");
  sib.inst(spv::Op::OpName,
           {entryPointId, mainWord, /* additional null in name */ 0});

  m.addDecoration(Decoration::getRelaxedPrecision(context), entryPointId);
  sib.inst(
      spv::Op::OpDecorate,
      {entryPointId, static_cast<uint32_t>(spv::Decoration::RelaxedPrecision)});

  const auto *i32Type = Type::getInt32(context);
  const uint32_t i32Id = context.getResultIdForType(i32Type);
  m.addType(i32Type, i32Id);
  sib.inst(spv::Op::OpTypeInt, {i32Id, 32, 1});

  const auto *voidType = Type::getVoid(context);
  const uint32_t voidId = context.getResultIdForType(voidType);
  m.addType(voidType, voidId);
  sib.inst(spv::Op::OpTypeVoid, {voidId});

  const auto *funcType = Type::getFunction(context, voidId, {voidId});
  const uint32_t funcTypeId = context.getResultIdForType(funcType);
  m.addType(funcType, funcTypeId);
  sib.inst(spv::Op::OpTypeFunction, {funcTypeId, voidId, voidId});

  const auto *i32Const = Constant::getInt32(context, i32Id, 42);
  const uint32_t constantId = context.takeNextId();
  m.addConstant(i32Const, constantId);
  sib.inst(spv::Op::OpConstant, {i32Id, constantId, 42});

  const uint32_t varId = context.takeNextId();
  m.addVariable(constructInst(
      spv::Op::OpVariable,
      {i32Id, varId, static_cast<uint32_t>(spv::StorageClass::Input)}));
  sib.inst(spv::Op::OpVariable,
           {i32Id, varId, static_cast<uint32_t>(spv::StorageClass::Input)});

  const uint32_t funcId = context.takeNextId();
  auto f = llvm::make_unique<Function>(
      voidId, funcId, spv::FunctionControlMask::MaskNone, funcTypeId);
  const uint32_t bbId = context.takeNextId();
  auto bb = llvm::make_unique<BasicBlock>(bbId);
  bb->appendInstruction(constructInst(spv::Op::OpReturn, {}));
  f->addBasicBlock(std::move(bb));
  m.addFunction(std::move(f));
  sib.inst(spv::Op::OpFunction, {voidId, funcId, 0, funcTypeId});
  sib.inst(spv::Op::OpLabel, {bbId});
  sib.inst(spv::Op::OpReturn, {});
  sib.inst(spv::Op::OpFunctionEnd, {});

  m.setBound(context.getNextId());

  std::vector<uint32_t> expected = sib.get();
  expected[3] = context.getNextId();

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  m.take(&ib);

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(m.isEmpty());
}

TEST(Structure, TakeModuleWithArrayAndConstantDependency) {
  SPIRVContext context;
  auto m = SPIRVModule();

  // Will fix up the id bound later.
  SimpleInstBuilder sib(0);

  // Add void type
  const auto *voidType = Type::getVoid(context);
  const uint32_t voidId = context.getResultIdForType(voidType);
  m.addType(voidType, voidId);

  // Add float type
  const auto *f32Type = Type::getFloat32(context);
  const uint32_t f32Id = context.getResultIdForType(f32Type);
  m.addType(f32Type, f32Id);

  // Add int64 type
  const auto *i64Type = Type::getInt64(context);
  const uint32_t i64Id = context.getResultIdForType(i64Type);
  m.addType(i64Type, i64Id);

  // Add int32 type
  const auto *i32Type = Type::getInt32(context);
  const uint32_t i32Id = context.getResultIdForType(i32Type);
  m.addType(i32Type, i32Id);

  // Add 32-bit integer constant (8)
  const auto *i32Const = Constant::getInt32(context, i32Id, 8);
  const uint32_t constantId = context.getResultIdForConstant(i32Const);
  m.addConstant(i32Const, constantId);

  // Add array of 8 32-bit integers type
  const auto *arrType = Type::getArray(context, i32Id, constantId);
  const uint32_t arrId = context.getResultIdForType(arrType);
  m.addType(arrType, arrId);
  m.setBound(context.getNextId());

  // Add another array of the same size. The constant does not need to be
  // redefined.
  const auto *arrStride = Decoration::getArrayStride(context, 4);
  const auto *secondArrType =
      Type::getArray(context, i32Id, constantId, {arrStride});
  const uint32_t secondArrId = context.getResultIdForType(arrType);
  m.addType(secondArrType, secondArrId);
  m.addDecoration(arrStride, secondArrId);
  m.setBound(context.getNextId());

  // Decorations
  sib.inst(
      spv::Op::OpDecorate,
      {secondArrId, static_cast<uint32_t>(spv::Decoration::ArrayStride), 4});
  // Now the expected order: int64, int32, void, float, constant(8), array
  sib.inst(spv::Op::OpTypeInt, {i64Id, 64, 1});
  sib.inst(spv::Op::OpTypeInt, {i32Id, 32, 1});
  sib.inst(spv::Op::OpTypeVoid, {voidId});
  sib.inst(spv::Op::OpTypeFloat, {f32Id, 32});
  sib.inst(spv::Op::OpConstant, {i32Id, constantId, 8});
  sib.inst(spv::Op::OpTypeArray, {arrId, i32Id, constantId});
  sib.inst(spv::Op::OpTypeArray, {secondArrId, i32Id, constantId});

  std::vector<uint32_t> expected = sib.get();
  expected[3] = context.getNextId();

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  m.take(&ib);

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(m.isEmpty());
}
} // anonymous namespace