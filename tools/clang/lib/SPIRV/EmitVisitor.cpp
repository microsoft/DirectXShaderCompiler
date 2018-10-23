//===--- EmitVisitor.cpp - SPIR-V Emit Visitor Implementation ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/EmitVisitor.h"
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/String.h"

namespace {
uint32_t zeroExtendTo32Bits(uint16_t value) {
  // TODO: The ordering of the 2 words depends on the endian-ness of the host
  // machine. Assuming Little Endian at the moment.
  struct two16Bits {
    uint16_t low;
    uint16_t high;
  };

  two16Bits result = {value, 0};
  return clang::spirv::cast::BitwiseCast<uint32_t, two16Bits>(result);
}

uint32_t signExtendTo32Bits(int16_t value) {
  // TODO: The ordering of the 2 words depends on the endian-ness of the host
  // machine. Assuming Little Endian at the moment.
  struct two16Bits {
    int16_t low;
    uint16_t high;
  };

  two16Bits result = {value, 0};

  // Sign bit is 1
  if (value >> 15) {
    result.high = 0xffff;
  }
  return clang::spirv::cast::BitwiseCast<uint32_t, two16Bits>(result);
}
} // anonymous namespace

namespace clang {
namespace spirv {

void EmitVisitor::emitDebugNameForInstruction(uint32_t resultId,
                                              llvm::StringRef debugName) {
  // Most instructions do not have a debug name associated with them.
  if (debugName.empty())
    return;

  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(spv::Op::OpName));
  curInst.push_back(resultId);
  encodeString(debugName);
  curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
  debugBinary.insert(debugBinary.end(), curInst.begin(), curInst.end());
}

void EmitVisitor::initInstruction(spv::Op op) {
  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(op));
}

void EmitVisitor::finalizeInstruction() {
  const auto op = static_cast<spv::Op>(curInst[0]);
  curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
  switch (op) {
  case spv::Op::OpCapability:
  case spv::Op::OpExtension:
  case spv::Op::OpExtInstImport:
  case spv::Op::OpMemoryModel:
  case spv::Op::OpEntryPoint:
  case spv::Op::OpExecutionMode:
  case spv::Op::OpExecutionModeId:
    preambleBinary.insert(preambleBinary.end(), curInst.begin(), curInst.end());
    break;
  case spv::Op::OpString:
  case spv::Op::OpSource:
  case spv::Op::OpSourceExtension:
  case spv::Op::OpSourceContinued:
  case spv::Op::OpName:
  case spv::Op::OpMemberName:
  case spv::Op::OpModuleProcessed:
    debugBinary.insert(debugBinary.end(), curInst.begin(), curInst.end());
    break;
  case spv::Op::OpDecorate:
  case spv::Op::OpDecorateId:
  case spv::Op::OpMemberDecorate:
  case spv::Op::OpGroupDecorate:
  case spv::Op::OpGroupMemberDecorate:
  case spv::Op::OpDecorationGroup:
  case spv::Op::OpDecorateStringGOOGLE:
  case spv::Op::OpMemberDecorateStringGOOGLE:
    annotationsBinary.insert(annotationsBinary.end(), curInst.begin(),
                             curInst.end());
    break;
  default:
    mainBinary.insert(mainBinary.end(), curInst.begin(), curInst.end());
    break;
  }
}

void EmitVisitor::encodeString(llvm::StringRef value) {
  const auto &words = string::encodeSPIRVString(value);
  curInst.insert(curInst.end(), words.begin(), words.end());
}

bool EmitVisitor::visit(SpirvModule *m, Phase phase) {
  // No pre or post ops for SpirvModule.
  return true;
}

bool EmitVisitor::visit(SpirvFunction *fn, Phase phase) {
  assert(fn);

  // Before emitting the function
  if (phase == Visitor::Phase::Init) {
    // Emit OpFunction
    initInstruction(spv::Op::OpFunction);
    curInst.push_back(fn->getReturnTypeId());
    curInst.push_back(fn->getResultId());
    curInst.push_back(
        static_cast<uint32_t>(spv::FunctionControlMask::MaskNone));
    curInst.push_back(fn->getFunctionTypeId());
    finalizeInstruction();
    emitDebugNameForInstruction(fn->getResultId(), fn->getFunctionName());
  }
  // After emitting the function
  else if (phase == Visitor::Phase::Done) {
    // Emit OpFunctionEnd
    initInstruction(spv::Op::OpFunctionEnd);
    finalizeInstruction();
  }

  return true;
}

bool EmitVisitor::visit(SpirvBasicBlock *bb, Phase phase) {
  assert(bb);

  // Before emitting the basic block.
  if (phase == Visitor::Phase::Init) {
    // Emit OpLabel
    initInstruction(spv::Op::OpLabel);
    curInst.push_back(bb->getLabelId());
    finalizeInstruction();
    emitDebugNameForInstruction(bb->getLabelId(), bb->getName());
  }
  // After emitting the basic block
  else if (phase == Visitor::Phase::Done) {
    assert(bb->hasTerminator());
  }
  return true;
}

bool EmitVisitor::visit(SpirvCapability *cap) {
  initInstruction(cap->getopcode());
  curInst.push_back(static_cast<uint32_t>(cap->getCapability()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtension *ext) {
  initInstruction(ext->getopcode());
  encodeString(ext->getExtensionName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInstImport *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultId());
  encodeString(inst->getExtendedInstSetName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvMemoryModel *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getAddressingModel()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryModel()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvEntryPoint *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getExecModel()));
  curInst.push_back(inst->getEntryPoint()->getResultId());
  encodeString(inst->getEntryPointName());
  for (auto *var : inst->getInterface())
    curInst.push_back(var->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvExecutionMode *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getEntryPoint()->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionMode()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvString *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultId());
  encodeString(inst->getString());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSource *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getSourceLanguage()));
  curInst.push_back(static_cast<uint32_t>(inst->getVersion()));
  if (inst->hasFile())
    curInst.push_back(inst->getFile()->getResultId());
  if (!inst->getSource().empty()) {
    // Note: in order to improve performance and avoid multiple copies, we
    // encode this (potentially large) string directly into the debugBinary.
    const auto &words = string::encodeSPIRVString(inst->getSource());
    const auto numWordsInInstr = curInst.size() + words.size();
    curInst[0] |= static_cast<uint32_t>(numWordsInInstr) << 16;
    debugBinary.insert(debugBinary.end(), curInst.begin(), curInst.end());
    debugBinary.insert(debugBinary.end(), words.begin(), words.end());
  }
  return true;
}

bool EmitVisitor::visit(SpirvModuleProcessed *inst) {
  initInstruction(inst->getopcode());
  encodeString(inst->getProcess());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvDecoration *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getTarget()->getResultId());
  if (inst->isMemberDecoration())
    curInst.push_back(inst->getMemberIndex());
  curInst.push_back(static_cast<uint32_t>(inst->getDecoration()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvVariable *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getStorageClass()));
  if (inst->hasInitializer())
    curInst.push_back(inst->getInitializer()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvFunctionParameter *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoopMerge *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getMergeBlock()->getLabelId());
  curInst.push_back(inst->getContinueTarget()->getLabelId());
  curInst.push_back(static_cast<uint32_t>(inst->getLoopControlMask()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSelectionMerge *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getMergeBlock()->getLabelId());
  curInst.push_back(static_cast<uint32_t>(inst->getSelectionControlMask()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBranch *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getTargetLabel()->getLabelId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBranchConditional *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getCondition()->getResultId());
  curInst.push_back(inst->getTrueLabel()->getLabelId());
  curInst.push_back(inst->getFalseLabel()->getLabelId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvKill *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvReturn *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSwitch *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getSelector()->getResultId());
  curInst.push_back(inst->getDefaultLabel()->getLabelId());
  for (const auto &target : inst->getTargets()) {
    curInst.push_back(target.first);
    curInst.push_back(target.second->getLabelId());
  }
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvUnreachable *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvAccessChain *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase()->getResultId());
  for (const auto index : inst->getIndexes())
    curInst.push_back(index->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvAtomic *inst) {
  const auto op = inst->getopcode();
  initInstruction(op);
  if (op != spv::Op::OpAtomicStore && op != spv::Op::OpAtomicFlagClear) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(inst->getResultId());
  }
  curInst.push_back(inst->getPointer()->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  if (inst->hasComparator())
    curInst.push_back(static_cast<uint32_t>(inst->getMemorySemanticsUnequal()));
  if (inst->hasValue())
    curInst.push_back(inst->getValue()->getResultId());
  if (inst->hasComparator())
    curInst.push_back(inst->getComparator()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBarrier *inst) {
  initInstruction(inst->getopcode());
  if (inst->isControlBarrier())
    curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getOperand1()->getResultId());
  curInst.push_back(inst->getOperand2()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldExtract *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase()->getResultId());
  curInst.push_back(inst->getOffset()->getResultId());
  curInst.push_back(inst->getCount()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldInsert *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase()->getResultId());
  curInst.push_back(inst->getInsert()->getResultId());
  curInst.push_back(inst->getOffset()->getResultId());
  curInst.push_back(inst->getCount()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantBoolean *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantInteger *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  // 16-bit cases
  if (inst->getBitwidth() == 16) {
    if (inst->isSigned()) {
      curInst.push_back(signExtendTo32Bits(inst->getSignedInt16Value()));
    } else {
      curInst.push_back(zeroExtendTo32Bits(inst->getUnsignedInt16Value()));
    }
  }
  // 32-bit cases
  else if (inst->getBitwidth() == 32) {
    if (inst->isSigned()) {
      curInst.push_back(
          cast::BitwiseCast<uint32_t, int32_t>(inst->getSignedInt32Value()));
    } else {
      curInst.push_back(inst->getUnsignedInt32Value());
    }
  }
  // 64-bit cases
  else {
    struct wideInt {
      uint32_t word0;
      uint32_t word1;
    };
    wideInt words;
    if (inst->isSigned()) {
      words = cast::BitwiseCast<wideInt, int64_t>(inst->getSignedInt64Value());
    } else {
      words =
          cast::BitwiseCast<wideInt, uint64_t>(inst->getUnsignedInt64Value());
    }
    curInst.push_back(words.word0);
    curInst.push_back(words.word1);
  }
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantFloat *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  if (inst->getBitwidth() == 16) {
    // According to the SPIR-V Spec:
    // When the type's bit width is less than 32-bits, the literal's value
    // appears in the low-order bits of the word, and the high-order bits must
    // be 0 for a floating-point type.
    curInst.push_back(zeroExtendTo32Bits(inst->getValue16()));
  } else if (inst->getBitwidth() == 32) {
    curInst.push_back(cast::BitwiseCast<uint32_t, float>(inst->getValue32()));
  } else {
    // TODO: The ordering of the 2 words depends on the endian-ness of the host
    // machine.
    struct wideFloat {
      uint32_t word0;
      uint32_t word1;
    };
    wideFloat words = cast::BitwiseCast<wideFloat, double>(inst->getValue64());
    curInst.push_back(words.word0);
    curInst.push_back(words.word1);
  }
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantComposite *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  for (const auto constituent : inst->getConstituents())
    curInst.push_back(constituent->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantNull *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvComposite *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  for (const auto constituent : inst->getConstituents())
    curInst.push_back(constituent->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeExtract *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getComposite()->getResultId());
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeInsert *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getObject()->getResultId());
  curInst.push_back(inst->getComposite()->getResultId());
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvEmitVertex *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvEndPrimitive *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInst *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getInstructionSet()->getResultId());
  curInst.push_back(inst->getInstruction());
  for (const auto operand : inst->getOperands())
    curInst.push_back(operand->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvFunctionCall *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getFunction()->getResultId());
  for (const auto arg : inst->getArgs())
    curInst.push_back(arg->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(inst->getArg1()->getResultId());
  curInst.push_back(inst->getArg2()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformElect *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  if (inst->hasGroupOp())
    curInst.push_back(static_cast<uint32_t>(inst->getGroupOp()));
  curInst.push_back(inst->getArg()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageOp *inst) {
  initInstruction(inst->getopcode());

  if (!inst->isImageWrite()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(inst->getResultId());
  }

  curInst.push_back(inst->getImage()->getResultId());
  curInst.push_back(inst->getCoordinate()->getResultId());

  if (inst->isImageWrite())
    curInst.push_back(inst->getTexelToWrite()->getResultId());

  if (inst->hasDref())
    curInst.push_back(inst->getDref()->getResultId());
  if (inst->hasComponent())
    curInst.push_back(inst->getComponent()->getResultId());
  if (inst->getImageOperandsMask() != spv::ImageOperandsMask::MaskNone) {
    curInst.push_back(static_cast<uint32_t>(inst->getImageOperandsMask()));
    if (inst->hasBias())
      curInst.push_back(inst->getBias()->getResultId());
    if (inst->hasLod())
      curInst.push_back(inst->getLod()->getResultId());
    if (inst->hasGrad()) {
      curInst.push_back(inst->getGradDx()->getResultId());
      curInst.push_back(inst->getGradDy()->getResultId());
    }
    if (inst->hasConstOffset())
      curInst.push_back(inst->getConstOffset()->getResultId());
    if (inst->hasOffset())
      curInst.push_back(inst->getOffset()->getResultId());
    if (inst->hasConstOffsets())
      curInst.push_back(inst->getConstOffsets()->getResultId());
    if (inst->hasSample())
      curInst.push_back(inst->getSample()->getResultId());
    if (inst->hasMinLod())
      curInst.push_back(inst->getMinLod()->getResultId());
  }
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageQuery *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage()->getResultId());
  if (inst->hasCoordinate())
    curInst.push_back(inst->getCoordinate()->getResultId());
  if (inst->hasLod())
    curInst.push_back(inst->getLod()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageSparseTexelsResident *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getResidentCode()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageTexelPointer *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage()->getResultId());
  curInst.push_back(inst->getCoordinate()->getResultId());
  curInst.push_back(inst->getSample()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoad *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getPointer()->getResultId());
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSampledImage *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage()->getResultId());
  curInst.push_back(inst->getSampler()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSelect *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getCondition()->getResultId());
  curInst.push_back(inst->getTrueObject()->getResultId());
  curInst.push_back(inst->getFalseObject()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(inst->getOperand1()->getResultId());
  curInst.push_back(inst->getOperand2()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(inst->getOperand()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvStore *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getPointer()->getResultId());
  curInst.push_back(inst->getObject()->getResultId());
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getOperand()->getResultId());
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvVectorShuffle *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getVec1()->getResultId());
  curInst.push_back(inst->getVec2()->getResultId());
  for (const auto component : inst->getComponents())
    curInst.push_back(component);
  finalizeInstruction();
  emitDebugNameForInstruction(inst->getResultId(), inst->getDebugName());
  return true;
}

} // end namespace spirv
} // end namespace clang
