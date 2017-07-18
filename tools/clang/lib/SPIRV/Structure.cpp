//===--- Structure.cpp - SPIR-V representation structures -----*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Structure.h"

namespace clang {
namespace spirv {

namespace {
constexpr uint32_t kGeneratorNumber = 14;
constexpr uint32_t kToolVersion = 0;

bool isTerminator(spv::Op opcode) {
  switch (opcode) {
  case spv::Op::OpKill:
  case spv::Op::OpUnreachable:
  case spv::Op::OpBranch:
  case spv::Op::OpBranchConditional:
  case spv::Op::OpSwitch:
  case spv::Op::OpReturn:
  case spv::Op::OpReturnValue:
    return true;
  default:
    return false;
  }
}
} // namespace

BasicBlock::BasicBlock(BasicBlock &&that)
    : labelId(that.labelId), instructions(std::move(that.instructions)) {
  that.clear();
}

BasicBlock &BasicBlock::operator=(BasicBlock &&that) {
  labelId = that.labelId;
  instructions = std::move(that.instructions);

  that.clear();

  return *this;
}

void BasicBlock::take(InstBuilder *builder) {
  // Make sure we have a terminator instruction at the end.
  // TODO: This is a little bit ugly. It suggests that we should put the opcode
  // in the Instruction struct. But fine for now.
  assert(!instructions.empty() && isTerminator(static_cast<spv::Op>(
                                      instructions.back().front() & 0xffff)));
  builder->opLabel(labelId).x();
  for (auto &inst : instructions) {
    builder->getConsumer()(std::move(inst));
  }
  clear();
}

Function::Function(Function &&that)
    : resultType(that.resultType), resultId(that.resultId),
      funcControl(that.funcControl), funcType(that.funcType),
      parameters(std::move(that.parameters)), blocks(std::move(that.blocks)) {
  that.clear();
}

Function &Function::operator=(Function &&that) {
  resultType = that.resultType;
  resultId = that.resultId;
  funcControl = that.funcControl;
  funcType = that.funcType;
  parameters = std::move(that.parameters);
  blocks = std::move(that.blocks);

  that.clear();

  return *this;
}

void Function::clear() {
  resultType = 0;
  resultId = 0;
  funcControl = spv::FunctionControlMask::MaskNone;
  funcType = 0;
  parameters.clear();
  blocks.clear();
}

void Function::take(InstBuilder *builder) {
  builder->opFunction(resultType, resultId, funcControl, funcType).x();
  for (auto &param : parameters) {
    builder->opFunctionParameter(param.first, param.second).x();
  }
  for (auto &block : blocks) {
    block->take(builder);
  }
  builder->opFunctionEnd().x();
  clear();
}

Header::Header()
    : magicNumber(spv::MagicNumber), version(spv::Version),
      generator((kGeneratorNumber << 16) | kToolVersion), bound(0),
      reserved(0) {}

void Header::collect(const WordConsumer &consumer) {
  std::vector<uint32_t> words;
  words.push_back(magicNumber);
  words.push_back(version);
  words.push_back(generator);
  words.push_back(bound);
  words.push_back(reserved);
  consumer(std::move(words));
}

bool SPIRVModule::isEmpty() const {
  return header.bound == 0 && capabilities.empty() && extensions.empty() &&
         extInstSets.empty() && !addressingModel.hasValue() &&
         !memoryModel.hasValue() && entryPoints.empty() &&
         executionModes.empty() && debugNames.empty() && decorations.empty() &&
         functions.empty();
}

void SPIRVModule::clear() {
  header.bound = 0;
  capabilities.clear();
  extensions.clear();
  extInstSets.clear();
  addressingModel = llvm::None;
  memoryModel = llvm::None;
  entryPoints.clear();
  executionModes.clear();
  debugNames.clear();
  decorations.clear();
  functions.clear();
}

void SPIRVModule::take(InstBuilder *builder) {
  const auto &consumer = builder->getConsumer();

  // Order matters here.
  header.collect(consumer);

  for (auto &cap : capabilities) {
    builder->opCapability(cap).x();
  }

  for (auto &ext : extensions) {
    builder->opExtension(ext).x();
  }

  for (auto &inst : extInstSets) {
    builder->opExtInstImport(inst.resultId, inst.setName).x();
  }

  if (addressingModel.hasValue() && memoryModel.hasValue()) {
    builder->opMemoryModel(*addressingModel, *memoryModel).x();
  }

  for (auto &inst : entryPoints) {
    builder
        ->opEntryPoint(inst.executionModel, inst.targetId,
                       std::move(inst.targetName), std::move(inst.interfaces))
        .x();
  }

  for (auto &inst : executionModes) {
    consumer(std::move(inst));
  }

  for (auto &inst : debugNames) {
    if (inst.memberIndex.hasValue()) {
      builder
          ->opMemberName(inst.targetId, *inst.memberIndex, std::move(inst.name))
          .x();
    } else {
      builder->opName(inst.targetId, std::move(inst.name)).x();
    }
  }

  for (const auto &d : decorations) {
    consumer(d.decoration.withTargetId(d.targetId));
  }

  // TODO: handle the interdependency between types and constants

  for (const auto &t : types) {
    consumer(t.first->withResultId(t.second));
  }

  for (auto &c : constants) {
    consumer(std::move(c.constant));
  }

  // TODO: global variables

  for (uint32_t i = 0; i < functions.size(); ++i) {
    functions[i]->take(builder);
  }

  clear();
}

} // end namespace spirv
} // end namespace clang