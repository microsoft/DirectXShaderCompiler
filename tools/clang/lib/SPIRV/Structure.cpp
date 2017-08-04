//===--- Structure.cpp - SPIR-V representation structures -----*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Structure.h"

#include "BlockReadableOrder.h"

namespace clang {
namespace spirv {

namespace {
constexpr uint32_t kGeneratorNumber = 14;
constexpr uint32_t kToolVersion = 0;
} // namespace

// === Instruction implementations ===

spv::Op Instruction::getOpcode() const {
  if (!isEmpty()) {
    return static_cast<spv::Op>(words.front() & spv::OpCodeMask);
  }

  return spv::Op::Max;
}

bool Instruction::isTerminator() const {
  switch (getOpcode()) {
  case spv::Op::OpBranch:
  case spv::Op::OpBranchConditional:
  case spv::Op::OpReturn:
  case spv::Op::OpReturnValue:
  case spv::Op::OpSwitch:
  case spv::Op::OpKill:
  case spv::Op::OpUnreachable:
    return true;
  default:
    return false;
  }
}

// === Basic block implementations ===

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

void BasicBlock::clear() {
  labelId = 0;
  instructions.clear();
}

void BasicBlock::take(InstBuilder *builder) {
  // Make sure we have a terminator instruction at the end.
  assert(isTerminated() && "found basic block without terminator");

  builder->opLabel(labelId).x();

  for (auto &inst : instructions) {
    builder->getConsumer()(inst.take());
  }

  clear();
}

bool BasicBlock::isTerminated() const {
  return !instructions.empty() && instructions.back().isTerminator();
}

// === Function implementations ===

Function::Function(Function &&that)
    : resultType(that.resultType), resultId(that.resultId),
      funcControl(that.funcControl), funcType(that.funcType),
      parameters(std::move(that.parameters)),
      variables(std::move(that.variables)), blocks(std::move(that.blocks)) {
  that.clear();
}

Function &Function::operator=(Function &&that) {
  resultType = that.resultType;
  resultId = that.resultId;
  funcControl = that.funcControl;
  funcType = that.funcType;
  parameters = std::move(that.parameters);
  variables = std::move(that.variables);
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
  variables.clear();
  blocks.clear();
}

void Function::take(InstBuilder *builder) {
  builder->opFunction(resultType, resultId, funcControl, funcType).x();

  // Write out all parameters.
  for (auto &param : parameters) {
    builder->opFunctionParameter(param.first, param.second).x();
  }

  if (!variables.empty()) {
    assert(!blocks.empty());
  }

  // Preprend all local variables to the entry block.
  // This is necessary since SPIR-V requires all local variables to be defined
  // at the very begining of the entry block.
  // We need to do it in the reverse order to guarantee variables have the
  // same definition order in SPIR-V as in the source code.
  for (auto it = variables.rbegin(), ie = variables.rend(); it != ie; ++it) {
    blocks.front()->prependInstruction(std::move(*it));
  }

  // Collect basic blocks in a human-readable order that satisfies SPIR-V
  // validation rules.
  std::vector<BasicBlock *> orderedBlocks;
  if (!blocks.empty()) {
    BlockReadableOrderVisitor([&orderedBlocks](BasicBlock *block) {
      orderedBlocks.push_back(block);
    }).visit(blocks.front().get());
  }

  // Write out all basic blocks.
  for (auto *block : orderedBlocks) {
    block->take(builder);
  }

  builder->opFunctionEnd().x();
  clear();
}

void Function::addVariable(uint32_t varType, uint32_t varId,
                           llvm::Optional<uint32_t> init) {
  variables.emplace_back(
      InstBuilder(nullptr)
          .opVariable(varType, varId, spv::StorageClass::Function, init)
          .take());
}

// === Module components implementations ===

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

// === Module implementations ===

bool SPIRVModule::isEmpty() const {
  return header.bound == 0 && capabilities.empty() && extensions.empty() &&
         extInstSets.empty() && !addressingModel.hasValue() &&
         !memoryModel.hasValue() && entryPoints.empty() &&
         executionModes.empty() && debugNames.empty() && decorations.empty() &&
         types.empty() && constants.empty() && variables.empty() &&
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
  types.clear();
  constants.clear();
  variables.clear();

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
                       std::move(inst.targetName), inst.interfaces)
        .x();
  }

  for (auto &inst : executionModes) {
    consumer(inst.take());
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

  // Note on interdependence of types and constants:
  // There is only one type (OpTypeArray) that requires the result-id of a
  // constant. As a result, the constant integer should be defined before the
  // array is defined. The integer type should also be defined before the
  // constant integer is defined.

  // First define all integer types
  takeIntegerTypes(builder);

  for (const auto &t : types) {
    // If we have an array type, we must first define the integer constant that
    // defines its length.
    if (t.first->isArrayType()) {
      takeConstantForArrayType(*t.first, builder);
    }

    consumer(t.first->withResultId(t.second));
  }

  for (const auto &c : constants) {
    consumer(c.first->withResultId(c.second));
  }

  for (auto &v : variables) {
    consumer(v.take());
  }

  for (uint32_t i = 0; i < functions.size(); ++i) {
    functions[i]->take(builder);
  }

  clear();
}

void SPIRVModule::takeIntegerTypes(InstBuilder *ib) {
  const auto &consumer = ib->getConsumer();
  // If it finds any integer type, feeds it into the consumer, and removes it
  // from the types collection.
  types.remove_if([&consumer](std::pair<const Type *, uint32_t> &item) {
    const bool isInteger = item.first->isIntegerType();
    if (isInteger)
      consumer(item.first->withResultId(item.second));
    return isInteger;
  });
}

void SPIRVModule::takeConstantForArrayType(const Type &arrType,
                                           InstBuilder *ib) {
  assert(arrType.isArrayType());

  const auto &consumer = ib->getConsumer();
  const uint32_t arrayLengthResultId = arrType.getArgs().back();

  // If it finds the constant, feeds it into the consumer, and removes it
  // from the constants collection.
  constants.remove_if([&consumer, arrayLengthResultId](
      std::pair<const Constant *, uint32_t> &item) {
    const bool isArrayLengthConstant = (item.second == arrayLengthResultId);
    if (isArrayLengthConstant)
      consumer(item.first->withResultId(item.second));
    return isArrayLengthConstant;
  });
}

} // end namespace spirv
} // end namespace clang