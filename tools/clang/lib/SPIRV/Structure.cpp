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

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#else
namespace clang {
uint32_t getGitCommitCount() { return 0; }
const char *getGitCommitHash() { return "<unknown-hash>"; }
} // namespace clang
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

namespace clang {
namespace spirv {

namespace {
constexpr uint32_t kGeneratorNumber = 14;
constexpr uint32_t kToolVersion = 0;

/// Chops the given original string into multiple smaller ones to make sure they
/// can be encoded in a sequence of OpSourceContinued instructions following an
/// OpSource instruction.
void chopString(llvm::StringRef original,
                llvm::SmallVectorImpl<llvm::StringRef> *chopped) {
  const uint32_t maxCharInOpSource = 0xFFFFu - 5u; // Minus operands and nul
  const uint32_t maxCharInContinue = 0xFFFFu - 2u; // Minus opcode and nul

  chopped->clear();
  if (original.size() > maxCharInOpSource) {
    chopped->push_back(llvm::StringRef(original.data(), maxCharInOpSource));
    original = llvm::StringRef(original.data() + maxCharInOpSource,
                               original.size() - maxCharInOpSource);
    while (original.size() > maxCharInContinue) {
      chopped->push_back(llvm::StringRef(original.data(), maxCharInContinue));
      original = llvm::StringRef(original.data() + maxCharInContinue,
                                 original.size() - maxCharInContinue);
    }
    if (!original.empty()) {
      chopped->push_back(original);
    }
  } else if (!original.empty()) {
    chopped->push_back(original);
  }
}
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
    : labelId(that.labelId), debugName(that.debugName),
      instructions(std::move(that.instructions)) {
  that.clear();
}

BasicBlock &BasicBlock::operator=(BasicBlock &&that) {
  labelId = that.labelId;
  debugName = that.debugName;
  instructions = std::move(that.instructions);

  that.clear();

  return *this;
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
  /*
  if (!blocks.empty()) {
    BlockReadableOrderVisitor(
        [&orderedBlocks](BasicBlock *block) { orderedBlocks.push_back(block); })
        .visit(blocks.front().get());
  }
  */

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

void Function::getReachableBasicBlocks(std::vector<BasicBlock *> *bbVec) const {
  /*
  if (!blocks.empty()) {
    BlockReadableOrderVisitor(
        [&bbVec](BasicBlock *block) { bbVec->push_back(block); })
        .visit(blocks.front().get());
  }
  */
}

// === Module components implementations ===

Header::Header()
    // We are using the unfied header, which shows spv::Version as the newest
    // version. But we need to stick to 1.0 for Vulkan consumption by default.
    : magicNumber(spv::MagicNumber), version(0x00010000),
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

bool DebugName::operator==(const DebugName &that) const {
  if (targetId == that.targetId && name == that.name) {
    if (memberIndex.hasValue()) {
      return that.memberIndex.hasValue() &&
             memberIndex.getValue() == that.memberIndex.getValue();
    }
    return !that.memberIndex.hasValue();
  }
  return false;
}

bool DebugName::operator<(const DebugName &that) const {
  // Sort according to target id first
  if (targetId != that.targetId)
    return targetId < that.targetId;

  if (memberIndex.hasValue()) {
    // Sort member decorations according to member index
    if (that.memberIndex.hasValue())
      return memberIndex.getValue() < that.memberIndex.getValue();
    // Decorations on the id itself goes before those on its members
    return false;
  }

  // Decorations on the id itself goes before those on its members
  if (that.memberIndex.hasValue())
    return true;

  return name < that.name;
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

  if (spirvOptions.targetEnv == "vulkan1.1")
    header.version = 0x00010300u;
  header.collect(consumer);

  for (auto &cap : capabilities) {
    builder->opCapability(cap).x();
  }

  for (auto &ext : extensions) {
    builder->opExtension(ext).x();
  }

  for (auto &inst : extInstSets) {
    builder->opExtInstImport(inst.second, inst.first).x();
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

  if (shaderModelVersion != 0) {
    llvm::Optional<uint32_t> fileName = llvm::None;

    if (spirvOptions.debugInfoFile && !sourceFileName.empty() &&
        sourceFileNameId) {
      builder->opString(sourceFileNameId, sourceFileName).x();
      fileName = sourceFileNameId;
    }

    llvm::Optional<llvm::StringRef> firstSnippet;
    if (spirvOptions.debugInfoSource) {
      llvm::SmallVector<llvm::StringRef, 2> choppedSrcCode;
      chopString(sourceFileContent, &choppedSrcCode);
      if (!choppedSrcCode.empty()) {
        firstSnippet = llvm::Optional<llvm::StringRef>(choppedSrcCode.front());
      }

      builder
          ->opSource(spv::SourceLanguage::HLSL, shaderModelVersion, fileName,
                     firstSnippet)
          .x();

      for (uint32_t i = 1; i < choppedSrcCode.size(); ++i) {
        builder->opSourceContinued(choppedSrcCode[i]).x();
      }
    } else {
      builder
          ->opSource(spv::SourceLanguage::HLSL, shaderModelVersion, fileName,
                     firstSnippet)
          .x();
    }
  }

  // BasicBlock debug names should be emitted only for blocks that are
  // reachable.
  // The debug name for a basic block is stored in the basic block object.
  std::vector<BasicBlock *> reachableBasicBlocks;
  for (const auto &fn : functions)
    fn->getReachableBasicBlocks(&reachableBasicBlocks);
  for (BasicBlock *bb : reachableBasicBlocks)
    if (!bb->getDebugName().empty())
      builder->opName(bb->getLabelId(), bb->getDebugName()).x();

  // Emit other debug names
  for (auto &inst : debugNames) {
    if (inst.memberIndex.hasValue()) {
      builder
          ->opMemberName(inst.targetId, *inst.memberIndex, std::move(inst.name))
          .x();
    } else {
      builder->opName(inst.targetId, std::move(inst.name)).x();
    }
  }

  if (spirvOptions.debugInfoTool && spirvOptions.targetEnv == "vulkan1.1") {
    // Emit OpModuleProcessed to indicate the commit information.
    std::string commitHash =
        std::string("dxc-commit-hash: ") + clang::getGitCommitHash();
    builder->opModuleProcessed(commitHash).x();

    // Emit OpModuleProcessed to indicate the command line options that were
    // used to generate this module.
    if (!spirvOptions.clOptions.empty()) {
      // Using this format: "dxc-cl-option: XXXXXX"
      std::string clOptionStr = "dxc-cl-option:" + spirvOptions.clOptions;
      builder->opModuleProcessed(clOptionStr).x();
    }
  }

  for (const auto &idDecorPair : decorations) {
    consumer(idDecorPair.second->withTargetId(idDecorPair.first));
  }

  // Note on interdependence of types and constants:
  // There is only one type (OpTypeArray) that requires the result-id of a
  // constant. As a result, the constant integer should be defined before the
  // array is defined. The integer type should also be defined before the
  // constant integer is defined.

  for (auto &v : typeConstant) {
    consumer(v.take());
  }

  for (auto &v : variables) {
    consumer(v.take());
  }

  for (uint32_t i = 0; i < functions.size(); ++i) {
    functions[i]->take(builder);
  }

  clear();
}

void SPIRVModule::addType(const Type *type, uint32_t resultId) {
  bool inserted = false;
  std::tie(std::ignore, inserted) = types.insert(type);
  if (inserted) {
    typeConstant.push_back(type->withResultId(resultId));
    for (const Decoration *d : type->getDecorations()) {
      addDecoration(d, resultId);
    }
  }
}

void SPIRVModule::addConstant(const Constant *constant, uint32_t resultId) {
  bool inserted = false;
  std::tie(std::ignore, inserted) = constants.insert(constant);
  if (inserted) {
    typeConstant.push_back(constant->withResultId(resultId));
  }
}

} // end namespace spirv
} // end namespace clang
