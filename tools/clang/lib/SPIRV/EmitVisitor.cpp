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
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/SpirvType.h"
#include "clang/SPIRV/String.h"

namespace {

constexpr uint32_t kGeneratorNumber = 14;
constexpr uint32_t kToolVersion = 0;

/// The alignment for 4-component float vectors.
constexpr uint32_t kStd140Vec4Alignment = 16u;

/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}

/// Returns true if the given vector type (of the given size) crosses the
/// 4-component vector boundary if placed at the given offset.
bool improperStraddle(const clang::spirv::VectorType *type, int size,
                      int offset) {
  return size <= 16 ? offset / 16 != (offset + size - 1) / 16
                    : offset % 16 != 0;
}

bool isAKindOfStructuredOrByteBuffer(const clang::spirv::SpirvType *type) {
  // Strip outer arrayness first
  while (llvm::isa<clang::spirv::ArrayType>(type))
    type = llvm::cast<clang::spirv::ArrayType>(type)->getElementType();

  // They are structures with the first member that is of RuntimeArray type.
  if (auto *structType = llvm::dyn_cast<clang::spirv::StructType>(type))
    return structType->getFields().size() == 1 &&
           llvm::isa<clang::spirv::RuntimeArrayType>(
               structType->getFields()[0].type);

  return false;
}

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

EmitVisitor::Header::Header(uint32_t bound_)
    // We are using the unfied header, which shows spv::Version as the newest
    // version. But we need to stick to 1.0 for Vulkan consumption by default.
    : magicNumber(spv::MagicNumber), version(0x00010000),
      generator((kGeneratorNumber << 16) | kToolVersion), bound(bound_),
      reserved(0) {}

std::vector<uint32_t> EmitVisitor::Header::takeBinary() {
  std::vector<uint32_t> words;
  words.push_back(magicNumber);
  words.push_back(version);
  words.push_back(generator);
  words.push_back(bound);
  words.push_back(reserved);
  return words;
}

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

void EmitVisitor::initInstruction(SpirvInstruction *inst) {
  if (inst->hasResultType()) {
    const uint32_t resultTypeId =
        typeHandler.emitType(inst->getResultType(), inst->getLayoutRule());
    inst->setResultTypeId(resultTypeId);
  }
  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(inst->getopcode()));
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

std::vector<uint32_t> EmitVisitor::takeBinary() {
  std::vector<uint32_t> result;
  Header header(takeNextId());
  auto headerBinary = header.takeBinary();
  result.insert(result.end(), headerBinary.begin(), headerBinary.end());
  result.insert(result.end(), preambleBinary.begin(), preambleBinary.end());
  result.insert(result.end(), debugBinary.begin(), debugBinary.end());
  result.insert(result.end(), annotationsBinary.begin(),
                annotationsBinary.end());
  result.insert(result.end(), typeConstantBinary.begin(),
                typeConstantBinary.end());
  result.insert(result.end(), mainBinary.begin(), mainBinary.end());
  return result;
}

void EmitVisitor::encodeString(llvm::StringRef value) {
  const auto &words = string::encodeSPIRVString(value);
  curInst.insert(curInst.end(), words.begin(), words.end());
}

bool EmitVisitor::visit(SpirvModule *m, Phase phase) {
  // No pre-visit operations needed for SpirvModule.

  if (phase == Visitor::Phase::Done)
    m->setBound(takeNextId());

  return true;
}

bool EmitVisitor::visit(SpirvFunction *fn, Phase phase) {
  assert(fn);

  // Before emitting the function
  if (phase == Visitor::Phase::Init) {
    const uint32_t returnTypeId =
        typeHandler.emitType(fn->getReturnType(), SpirvLayoutRule::Void);
    const uint32_t functionTypeId =
        typeHandler.emitType(fn->getFunctionType(), SpirvLayoutRule::Void);
    fn->setReturnTypeId(returnTypeId);
    fn->setFunctionTypeId(functionTypeId);

    // Emit OpFunction
    initInstruction(spv::Op::OpFunction);
    curInst.push_back(returnTypeId);
    curInst.push_back(getResultId<SpirvFunction>(fn));
    curInst.push_back(
        static_cast<uint32_t>(spv::FunctionControlMask::MaskNone));
    curInst.push_back(functionTypeId);
    finalizeInstruction();
    emitDebugNameForInstruction(getResultId<SpirvFunction>(fn),
                                fn->getFunctionName());
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
    curInst.push_back(getResultId<SpirvBasicBlock>(bb));
    finalizeInstruction();
    emitDebugNameForInstruction(getResultId<SpirvBasicBlock>(bb),
                                bb->getName());
  }
  // After emitting the basic block
  else if (phase == Visitor::Phase::Done) {
    assert(bb->hasTerminator());
  }
  return true;
}

bool EmitVisitor::visit(SpirvCapability *cap) {
  initInstruction(cap);
  curInst.push_back(static_cast<uint32_t>(cap->getCapability()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtension *ext) {
  initInstruction(ext);
  encodeString(ext->getExtensionName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInstImport *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  encodeString(inst->getExtendedInstSetName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvMemoryModel *inst) {
  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getAddressingModel()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryModel()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvEntryPoint *inst) {
  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getExecModel()));
  curInst.push_back(getResultId<SpirvFunction>(inst->getEntryPoint()));
  encodeString(inst->getEntryPointName());
  for (auto *var : inst->getInterface())
    curInst.push_back(getResultId<SpirvInstruction>(var));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvExecutionMode *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvFunction>(inst->getEntryPoint()));
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionMode()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvString *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  encodeString(inst->getString());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSource *inst) {
  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getSourceLanguage()));
  curInst.push_back(static_cast<uint32_t>(inst->getVersion()));
  if (inst->hasFile())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getFile()));
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
  initInstruction(inst);
  encodeString(inst->getProcess());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvDecoration *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst->getTarget()));
  if (inst->isMemberDecoration())
    curInst.push_back(inst->getMemberIndex());
  curInst.push_back(static_cast<uint32_t>(inst->getDecoration()));
  if (!inst->getParams().empty()) {
    curInst.insert(curInst.end(), inst->getParams().begin(),
                   inst->getParams().end());
  }
  if (!inst->getIdParams().empty()) {
    for (auto *paramInstr : inst->getIdParams())
      curInst.push_back(getResultId<SpirvInstruction>(paramInstr));
  }
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvVariable *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getStorageClass()));
  if (inst->hasInitializer())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getInitializer()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvFunctionParameter *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoopMerge *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getMergeBlock()));
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getContinueTarget()));
  curInst.push_back(static_cast<uint32_t>(inst->getLoopControlMask()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSelectionMerge *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getMergeBlock()));
  curInst.push_back(static_cast<uint32_t>(inst->getSelectionControlMask()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBranch *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getTargetLabel()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBranchConditional *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCondition()));
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getTrueLabel()));
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getFalseLabel()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvKill *inst) {
  initInstruction(inst);
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvReturn *inst) {
  initInstruction(inst);
  if (inst->hasReturnValue()) {
    curInst.push_back(getResultId<SpirvInstruction>(inst->getReturnValue()));
  }
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSwitch *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst->getSelector()));
  curInst.push_back(getResultId<SpirvBasicBlock>(inst->getDefaultLabel()));
  for (const auto &target : inst->getTargets()) {
    curInst.push_back(target.first);
    curInst.push_back(getResultId<SpirvBasicBlock>(target.second));
  }
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvUnreachable *inst) {
  initInstruction(inst);
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvAccessChain *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getBase()));
  for (const auto index : inst->getIndexes())
    curInst.push_back(getResultId<SpirvInstruction>(index));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvAtomic *inst) {
  const auto op = inst->getopcode();
  initInstruction(inst);
  if (op != spv::Op::OpAtomicStore && op != spv::Op::OpAtomicFlagClear) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getResultId<SpirvInstruction>(inst));
  }
  curInst.push_back(getResultId<SpirvInstruction>(inst->getPointer()));
  curInst.push_back(static_cast<uint32_t>(inst->getScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  if (inst->hasComparator())
    curInst.push_back(static_cast<uint32_t>(inst->getMemorySemanticsUnequal()));
  if (inst->hasValue())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getValue()));
  if (inst->hasComparator())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getComparator()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBarrier *inst) {
  initInstruction(inst);
  if (inst->isControlBarrier())
    curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand1()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand2()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldExtract *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getBase()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOffset()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCount()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldInsert *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getBase()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getInsert()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOffset()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCount()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantBoolean *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantInteger *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
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
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantFloat *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
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
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantComposite *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  for (auto constituent : inst->getConstituents())
    curInst.push_back(getResultId<SpirvInstruction>(constituent));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantNull *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvComposite *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  for (const auto constituent : inst->getConstituents())
    curInst.push_back(getResultId<SpirvInstruction>(constituent));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeExtract *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getComposite()));
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeInsert *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getObject()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getComposite()));
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvEmitVertex *inst) {
  initInstruction(inst);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvEndPrimitive *inst) {
  initInstruction(inst);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInst *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getInstruction());
  for (const auto operand : inst->getOperands())
    curInst.push_back(getResultId<SpirvInstruction>(operand));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvFunctionCall *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvFunction>(inst->getFunction()));
  for (const auto arg : inst->getArgs())
    curInst.push_back(getResultId<SpirvInstruction>(arg));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getArg1()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getArg2()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformElect *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  if (inst->hasGroupOp())
    curInst.push_back(static_cast<uint32_t>(inst->getGroupOp()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getArg()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageOp *inst) {
  initInstruction(inst);

  if (!inst->isImageWrite()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getResultId<SpirvInstruction>(inst));
  }

  curInst.push_back(getResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCoordinate()));

  if (inst->isImageWrite())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getTexelToWrite()));

  if (inst->hasDref())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getDref()));
  if (inst->hasComponent())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getComponent()));
  if (inst->getImageOperandsMask() != spv::ImageOperandsMask::MaskNone) {
    curInst.push_back(static_cast<uint32_t>(inst->getImageOperandsMask()));
    if (inst->hasBias())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getBias()));
    if (inst->hasLod())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getLod()));
    if (inst->hasGrad()) {
      curInst.push_back(getResultId<SpirvInstruction>(inst->getGradDx()));
      curInst.push_back(getResultId<SpirvInstruction>(inst->getGradDy()));
    }
    if (inst->hasConstOffset())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getConstOffset()));
    if (inst->hasOffset())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getOffset()));
    if (inst->hasConstOffsets())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getConstOffsets()));
    if (inst->hasSample())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getSample()));
    if (inst->hasMinLod())
      curInst.push_back(getResultId<SpirvInstruction>(inst->getMinLod()));
  }
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageQuery *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getImage()));
  if (inst->hasCoordinate())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getCoordinate()));
  if (inst->hasLod())
    curInst.push_back(getResultId<SpirvInstruction>(inst->getLod()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageSparseTexelsResident *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getResidentCode()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageTexelPointer *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCoordinate()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getSample()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoad *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getPointer()));
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSampledImage *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getSampler()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSelect *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getCondition()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getTrueObject()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getFalseObject()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand1()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand2()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvStore *inst) {
  initInstruction(inst);
  curInst.push_back(getResultId<SpirvInstruction>(inst->getPointer()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getObject()));
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getOperand()));
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvVectorShuffle *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getResultId<SpirvInstruction>(inst));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getVec1()));
  curInst.push_back(getResultId<SpirvInstruction>(inst->getVec2()));
  for (const auto component : inst->getComponents())
    curInst.push_back(component);
  finalizeInstruction();
  emitDebugNameForInstruction(getResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

// EmitTypeHandler ------

void EmitTypeHandler::initTypeInstruction(spv::Op op) {
  curTypeInst.clear();
  curTypeInst.push_back(static_cast<uint32_t>(op));
}

void EmitTypeHandler::finalizeTypeInstruction() {
  curTypeInst[0] |= static_cast<uint32_t>(curTypeInst.size()) << 16;
  typeConstantBinary->insert(typeConstantBinary->end(), curTypeInst.begin(),
                             curTypeInst.end());
}

uint32_t EmitTypeHandler::getResultIdForType(const SpirvType *type,
                                             SpirvLayoutRule rule,
                                             bool *alreadyExists) {
  assert(alreadyExists);
  // Check if the type has already been emitted.
  auto foundType = emittedTypes.find(type);
  if (foundType != emittedTypes.end()) {
    auto foundLayoutRule = foundType->second.find(rule);
    if (foundLayoutRule != foundType->second.end()) {
      *alreadyExists = true;
      return foundLayoutRule->second;
    }
  }

  *alreadyExists = false;
  const uint32_t id = takeNextIdFunction();
  emittedTypes[type][rule] = id;
  return id;
}

uint32_t EmitTypeHandler::emitType(const SpirvType *type,
                                   SpirvLayoutRule rule) {
  //
  // TODO: This method is currently missing decorations for types completely.
  //

  bool alreadyExists = false;
  const uint32_t id = getResultIdForType(type, rule, &alreadyExists);

  // If the type has already been emitted, we just need to return its
  // <result-id>.
  if (alreadyExists)
    return id;

  if (isa<VoidType>(type)) {
    initTypeInstruction(spv::Op::OpTypeVoid);
    curTypeInst.push_back(id);
    finalizeTypeInstruction();
  }
  // Boolean types
  else if (isa<BoolType>(type)) {
    initTypeInstruction(spv::Op::OpTypeBool);
    curTypeInst.push_back(id);
    finalizeTypeInstruction();
  }
  // Integer types
  else if (const auto *intType = dyn_cast<IntegerType>(type)) {
    initTypeInstruction(spv::Op::OpTypeInt);
    curTypeInst.push_back(id);
    curTypeInst.push_back(intType->getBitwidth());
    curTypeInst.push_back(intType->isSignedInt() ? 1 : 0);
    finalizeTypeInstruction();
  }
  // Float types
  else if (const auto *floatType = dyn_cast<FloatType>(type)) {
    initTypeInstruction(spv::Op::OpTypeFloat);
    curTypeInst.push_back(id);
    curTypeInst.push_back(floatType->getBitwidth());
    finalizeTypeInstruction();
  }
  // Vector types
  else if (const auto *vecType = dyn_cast<VectorType>(type)) {
    const uint32_t elementTypeId = emitType(vecType->getElementType(), rule);
    initTypeInstruction(spv::Op::OpTypeVector);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elementTypeId);
    curTypeInst.push_back(vecType->getElementCount());
    finalizeTypeInstruction();
  }
  // Matrix types
  else if (const auto *matType = dyn_cast<MatrixType>(type)) {
    const uint32_t vecTypeId = emitType(matType->getVecType(), rule);
    initTypeInstruction(spv::Op::OpTypeMatrix);
    curTypeInst.push_back(id);
    curTypeInst.push_back(vecTypeId);
    curTypeInst.push_back(matType->getVecCount());
    finalizeTypeInstruction();
    // Note that RowMajor and ColMajor decorations only apply to structure
    // members, and should not be handled here.
  }
  // Image types
  else if (const auto *imageType = dyn_cast<ImageType>(type)) {
    const uint32_t sampledTypeId = emitType(imageType->getSampledType(), rule);
    initTypeInstruction(spv::Op::OpTypeImage);
    curTypeInst.push_back(id);
    curTypeInst.push_back(sampledTypeId);
    curTypeInst.push_back(static_cast<uint32_t>(imageType->getDimension()));
    curTypeInst.push_back(static_cast<uint32_t>(imageType->getDepth()));
    curTypeInst.push_back(imageType->isArrayedImage() ? 1 : 0);
    curTypeInst.push_back(imageType->isMSImage() ? 1 : 0);
    curTypeInst.push_back(static_cast<uint32_t>(imageType->withSampler()));
    curTypeInst.push_back(static_cast<uint32_t>(imageType->getImageFormat()));
    finalizeTypeInstruction();
  }
  // Sampler types
  else if (const auto *samplerType = dyn_cast<SamplerType>(type)) {
    initTypeInstruction(spv::Op::OpTypeSampler);
    curTypeInst.push_back(id);
    finalizeTypeInstruction();
  }
  // SampledImage types
  else if (const auto *sampledImageType = dyn_cast<SampledImageType>(type)) {
    const uint32_t imageTypeId =
        emitType(sampledImageType->getImageType(), rule);
    initTypeInstruction(spv::Op::OpTypeSampledImage);
    curTypeInst.push_back(id);
    curTypeInst.push_back(imageTypeId);
    finalizeTypeInstruction();
  }
  // Array types
  else if (const auto *arrayType = dyn_cast<ArrayType>(type)) {
    // Emit the OpConstant instruction that is needed to get the result-id for
    // the array length.
    SpirvConstant *constant =
        spirvBuilder.getConstantUint32(arrayType->getElementCount());
    if (getResultId<SpirvInstruction>(constant) == 0) {
      constant->setResultId(takeNextIdFunction());
    }
    IntegerType constantIntType(32, 0);
    const uint32_t uint32TypeId = emitType(&constantIntType, rule);
    initTypeInstruction(spv::Op::OpConstant);
    curTypeInst.push_back(uint32TypeId);
    curTypeInst.push_back(getResultId<SpirvInstruction>(constant));
    curTypeInst.push_back(arrayType->getElementCount());
    finalizeTypeInstruction();

    // Emit the OpTypeArray instruction
    const uint32_t elemTypeId = emitType(arrayType->getElementType(), rule);
    initTypeInstruction(spv::Op::OpTypeArray);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elemTypeId);
    curTypeInst.push_back(getResultId<SpirvInstruction>(constant));
    finalizeTypeInstruction();

    // ArrayStride decoration is needed for array types, but we won't have
    // stride information for structured/byte buffers since they contain runtime
    // arrays.
    if (rule != SpirvLayoutRule::Void &&
        !isAKindOfStructuredOrByteBuffer(type)) {
      uint32_t stride = 0;
      (void)getAlignmentAndSize(type, rule, &stride);
      emitDecoration(id, spv::Decoration::ArrayStride, {stride});
    }
  }
  // RuntimeArray types
  else if (const auto *raType = dyn_cast<RuntimeArrayType>(type)) {
    const uint32_t elemTypeId = emitType(raType->getElementType(), rule);
    initTypeInstruction(spv::Op::OpTypeRuntimeArray);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elemTypeId);
    finalizeTypeInstruction();

    // ArrayStride decoration is needed for runtime array types.
    if (rule != SpirvLayoutRule::Void) {
      uint32_t stride = 0;
      (void)getAlignmentAndSize(type, rule, &stride);
      emitDecoration(id, spv::Decoration::ArrayStride, {stride});
    }
  }
  // Structure types
  else if (const auto *structType = dyn_cast<StructType>(type)) {
    llvm::SmallVector<uint32_t, 4> fieldTypeIds;
    for (auto &field : structType->getFields())
      fieldTypeIds.push_back(emitType(field.type, rule));
    initTypeInstruction(spv::Op::OpTypeStruct);
    curTypeInst.push_back(id);
    for (auto fieldTypeId : fieldTypeIds)
      curTypeInst.push_back(fieldTypeId);
    finalizeTypeInstruction();

    // Emit the layout decorations for the structure.
    emitLayoutDecorations(structType, rule);

    // Emit NonWritable decorations
    if (structType->isReadOnly())
      for (size_t i = 0; i < structType->getFields().size(); ++i)
        emitDecoration(id, spv::Decoration::NonWritable, {}, i);

    // Emit Block or BufferBlock decorations if necessary.
    auto interfaceType = structType->getInterfaceType();
    if (interfaceType == StructInterfaceType::StorageBuffer)
      emitDecoration(id, spv::Decoration::BufferBlock, {});
    else if (interfaceType == StructInterfaceType::UniformBuffer)
      emitDecoration(id, spv::Decoration::Block, {});
  }
  // Pointer types
  else if (const auto *ptrType = dyn_cast<SpirvPointerType>(type)) {
    const uint32_t pointeeType = emitType(ptrType->getPointeeType(), rule);
    initTypeInstruction(spv::Op::OpTypePointer);
    curTypeInst.push_back(id);
    curTypeInst.push_back(static_cast<uint32_t>(ptrType->getStorageClass()));
    curTypeInst.push_back(pointeeType);
    finalizeTypeInstruction();
  }
  // Function types
  else if (const auto *fnType = dyn_cast<FunctionType>(type)) {
    const uint32_t retTypeId = emitType(fnType->getReturnType(), rule);
    llvm::SmallVector<uint32_t, 4> paramTypeIds;
    for (auto *paramType : fnType->getParamTypes())
      paramTypeIds.push_back(emitType(paramType, rule));

    initTypeInstruction(spv::Op::OpTypeFunction);
    curTypeInst.push_back(id);
    curTypeInst.push_back(retTypeId);
    for (auto paramTypeId : paramTypeIds)
      curTypeInst.push_back(paramTypeId);
    finalizeTypeInstruction();
  }
  // Hybrid Function types
  else if (const auto *fnType = dyn_cast<HybridFunctionType>(type)) {
    const uint32_t retTypeId = emitType(fnType->getReturnType(), rule);
    llvm::SmallVector<uint32_t, 4> paramTypeIds;
    for (auto *paramType : fnType->getParamTypes())
      paramTypeIds.push_back(emitType(paramType, rule));

    initTypeInstruction(spv::Op::OpTypeFunction);
    curTypeInst.push_back(id);
    curTypeInst.push_back(retTypeId);
    for (auto paramTypeId : paramTypeIds)
      curTypeInst.push_back(paramTypeId);
    finalizeTypeInstruction();
  }
  // Unhandled types
  else {
    llvm_unreachable("unhandled type in emitType");
  }

  return id;
}

std::pair<uint32_t, uint32_t>
EmitTypeHandler::getAlignmentAndSize(const SpirvType *type,
                                     SpirvLayoutRule rule, uint32_t *stride) {
  // std140 layout rules:

  // 1. If the member is a scalar consuming N basic machine units, the base
  //    alignment is N.
  //
  // 2. If the member is a two- or four-component vector with components
  //    consuming N basic machine units, the base alignment is 2N or 4N,
  //    respectively.
  //
  // 3. If the member is a three-component vector with components consuming N
  //    basic machine units, the base alignment is 4N.
  //
  // 4. If the member is an array of scalars or vectors, the base alignment and
  //    array stride are set to match the base alignment of a single array
  //    element, according to rules (1), (2), and (3), and rounded up to the
  //    base alignment of a vec4. The array may have padding at the end; the
  //    base offset of the member following the array is rounded up to the next
  //    multiple of the base alignment.
  //
  // 5. If the member is a column-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of C column vectors with R
  //    components each, according to rule (4).
  //
  // 6. If the member is an array of S column-major matrices with C columns and
  //    R rows, the matrix is stored identically to a row of S X C column
  //    vectors with R components each, according to rule (4).
  //
  // 7. If the member is a row-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of R row vectors with C
  //    components each, according to rule (4).
  //
  // 8. If the member is an array of S row-major matrices with C columns and R
  //    rows, the matrix is stored identically to a row of S X R row vectors
  //    with C components each, according to rule (4).
  //
  // 9. If the member is a structure, the base alignment of the structure is N,
  //    where N is the largest base alignment value of any of its members, and
  //    rounded up to the base alignment of a vec4. The individual members of
  //    this substructure are then assigned offsets by applying this set of
  //    rules recursively, where the base offset of the first member of the
  //    sub-structure is equal to the aligned offset of the structure. The
  //    structure may have padding at the end; the base offset of the member
  //    following the sub-structure is rounded up to the next multiple of the
  //    base alignment of the structure.
  //
  // 10. If the member is an array of S structures, the S elements of the array
  //     are laid out in order, according to rule (9).
  //
  // This method supports multiple layout rules, all of them modifying the
  // std140 rules listed above:
  //
  // std430:
  // - Array base alignment and stride does not need to be rounded up to a
  //   multiple of 16.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.
  //
  // Relaxed std140/std430:
  // - Vector base alignment is set as its element type's base alignment.
  //
  // FxcCTBuffer:
  // - Vector base alignment is set as its element type's base alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  //
  // FxcSBuffer:
  // - Vector/matrix/array base alignment is set as its element type's base
  //   alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.

  { // Rule 1
    if (isa<BoolType>(type))
      return {4, 4};
    // Integer and Float types are NumericalType
    if (auto *numericType = dyn_cast<NumericalType>(type)) {
      switch (numericType->getBitwidth()) {
      case 64:
        return {8, 8};
      case 32:
        return {4, 4};
      case 16:
        return {2, 2};
      default:
        emitError("alignment and size calculation unimplemented for type");
        return {0, 0};
      }
    }
  }

  { // Rule 2 and 3
    if (auto *vecType = dyn_cast<VectorType>(type)) {
      uint32_t alignment = 0, size = 0;
      uint32_t elemCount = vecType->getElementCount();
      const SpirvType *elemType = vecType->getElementType();
      std::tie(alignment, size) = getAlignmentAndSize(elemType, rule, stride);
      // Use element alignment for fxc rules
      if (rule != SpirvLayoutRule::FxcCTBuffer &&
          rule != SpirvLayoutRule::FxcSBuffer)
        alignment = (elemCount == 3 ? 4 : elemCount) * size;

      return {alignment, elemCount * size};
    }
  }

  { // Rule 5 and 7
    if (auto *matType = dyn_cast<MatrixType>(type)) {
      const SpirvType *elemType = matType->getElementType();
      uint32_t rowCount = matType->numRows();
      uint32_t colCount = matType->numCols();
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, size) = getAlignmentAndSize(elemType, rule, stride);

      // Matrices are treated as arrays of vectors:
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      bool isRowMajor = matType->isRowMajorMat();

      const uint32_t vecStorageSize = isRowMajor ? colCount : rowCount;

      if (rule == SpirvLayoutRule::FxcSBuffer) {
        *stride = vecStorageSize * size;
        // Use element alignment for fxc structured buffers
        return {alignment, rowCount * colCount * size};
      }

      alignment *= (vecStorageSize == 3 ? 4 : vecStorageSize);
      if (rule == SpirvLayoutRule::GLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignment = roundToPow2(alignment, kStd140Vec4Alignment);
      }
      *stride = alignment;
      size = (isRowMajor ? rowCount : colCount) * alignment;

      return {alignment, size};
    }
  }

  // Rule 9
  if (auto *structType = dyn_cast<StructType>(type)) {
    // Special case for handling empty structs, whose size is 0 and has no
    // requirement over alignment (thus 1).
    if (structType->getFields().size() == 0)
      return {1, 0};

    uint32_t maxAlignment = 0;
    uint32_t structSize = 0;

    for (auto &field : structType->getFields()) {
      uint32_t memberAlignment = 0, memberSize = 0;
      std::tie(memberAlignment, memberSize) =
          getAlignmentAndSize(field.type, rule, stride);

      if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignUsingHLSLRelaxedLayout(field.type, memberSize, memberAlignment,
                                    &structSize);
      } else {
        structSize = roundToPow2(structSize, memberAlignment);
      }

      // Reset the current offset to the one specified in the source code
      // if exists. It's debatable whether we should do sanity check here.
      // If the developers want manually control the layout, we leave
      // everything to them.
      if (field.vkOffsetAttr) {
        structSize = field.vkOffsetAttr->getOffset();
      }

      // The base alignment of the structure is N, where N is the largest
      // base alignment value of any of its members...
      maxAlignment = std::max(maxAlignment, memberAlignment);
      structSize += memberSize;
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // ... and rounded up to the base alignment of a vec4.
      maxAlignment = roundToPow2(maxAlignment, kStd140Vec4Alignment);
    }

    if (rule != SpirvLayoutRule::FxcCTBuffer &&
        rule != SpirvLayoutRule::FxcSBuffer) {
      // The base offset of the member following the sub-structure is rounded up
      // to the next multiple of the base alignment of the structure.
      structSize = roundToPow2(structSize, maxAlignment);
    }
    return {maxAlignment, structSize};
  }

  // Rule 4, 6, 8, and 10
  if (auto *arrayType = dyn_cast<ArrayType>(type)) {
    const auto elemCount = arrayType->getElementCount();
    uint32_t alignment = 0, size = 0;
    std::tie(alignment, size) =
        getAlignmentAndSize(arrayType->getElementType(), rule, stride);

    if (rule == SpirvLayoutRule::FxcSBuffer) {
      *stride = size;
      // Use element alignment for fxc structured buffers
      return {alignment, size * elemCount};
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      alignment = roundToPow2(alignment, kStd140Vec4Alignment);
    }
    if (rule == SpirvLayoutRule::FxcCTBuffer) {
      // In fxc cbuffer/tbuffer packing rules, arrays does not affect the data
      // packing after it. But we still need to make sure paddings are inserted
      // internally if necessary.
      *stride = roundToPow2(size, alignment);
      size += *stride * (elemCount - 1);
    } else {
      // Need to round size up considering stride for scalar types
      size = roundToPow2(size, alignment);
      *stride = size; // Use size instead of alignment here for Rule 10
      size *= elemCount;
      // The base offset of the member following the array is rounded up to the
      // next multiple of the base alignment.
      size = roundToPow2(size, alignment);
    }

    return {alignment, size};
  }

  emitError("alignment and size calculation unimplemented for type");
  return {0, 0};
}

void EmitTypeHandler::alignUsingHLSLRelaxedLayout(const SpirvType *fieldType,
                                                  uint32_t fieldSize,
                                                  uint32_t fieldAlignment,
                                                  uint32_t *currentOffset) {
  if (auto *vecType = dyn_cast<VectorType>(fieldType)) {
    const SpirvType *elemType = vecType->getElementType();
    // Adjust according to HLSL relaxed layout rules.
    // Aligning vectors as their element types so that we can pack a float
    // and a float3 tightly together.
    uint32_t scalarAlignment = 0;
    std::tie(scalarAlignment, std::ignore) =
        getAlignmentAndSize(elemType, SpirvLayoutRule::Void, nullptr);
    if (scalarAlignment <= 4)
      fieldAlignment = scalarAlignment;

    *currentOffset = roundToPow2(*currentOffset, fieldAlignment);

    // Adjust according to HLSL relaxed layout rules.
    // Bump to 4-component vector alignment if there is a bad straddle
    if (improperStraddle(vecType, fieldSize, *currentOffset)) {
      fieldAlignment = kStd140Vec4Alignment;
      *currentOffset = roundToPow2(*currentOffset, fieldAlignment);
    }
  }
  // Cases where the field is not a vector
  else {
    *currentOffset = roundToPow2(*currentOffset, fieldAlignment);
  }
}

void EmitTypeHandler::emitLayoutDecorations(const StructType *structType,
                                            SpirvLayoutRule rule) {
  // Decorations for a type can be emitted after the type itself has been
  // visited, because we need the result-id of the type as the target of the
  // decoration.
  bool visited = false;
  const uint32_t typeResultId = getResultIdForType(structType, rule, &visited);
  assert(visited);

  uint32_t offset = 0, index = 0;
  for (auto &field : structType->getFields()) {
    const SpirvType *fieldType = field.type;
    uint32_t memberAlignment = 0, memberSize = 0, stride = 0;
    std::tie(memberAlignment, memberSize) =
        getAlignmentAndSize(fieldType, rule, &stride);

    // The next avaiable location after laying out the previous members
    const uint32_t nextLoc = offset;

    if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      alignUsingHLSLRelaxedLayout(fieldType, memberSize, memberAlignment,
                                  &offset);
    } else {
      offset = roundToPow2(offset, memberAlignment);
    }

    // The vk::offset attribute takes precedence over all.
    if (field.vkOffsetAttr) {
      offset = field.vkOffsetAttr->getOffset();
    }
    // The :packoffset() annotation takes precedence over normal layout
    // calculation.
    else if (field.packOffsetAttr) {
      const uint32_t packOffset = field.packOffsetAttr->Subcomponent * 16 +
                                  field.packOffsetAttr->ComponentOffset * 4;
      // Do minimal check to make sure the offset specified by packoffset does
      // not cause overlap.
      if (packOffset < nextLoc) {
        emitError("packoffset caused overlap with previous members",
                  field.packOffsetAttr->Loc);
      } else {
        offset = packOffset;
      }
    }

    // Each structure-type member must have an Offset Decoration.
    emitDecoration(typeResultId, spv::Decoration::Offset, {offset}, index);
    offset += memberSize;

    // Each structure-type member that is a matrix or array-of-matrices must be
    // decorated with
    // * A MatrixStride decoration, and
    // * one of the RowMajor or ColMajor Decorations.
    if (auto *arrayType = dyn_cast<ArrayType>(fieldType)) {
      // We have an array of matrices as a field, we need to decorate
      // MatrixStride on the field. So skip possible arrays here.
      fieldType = arrayType->getElementType();
    }

    // Non-floating point matrices are represented as arrays of vectors, and
    // therefore ColMajor and RowMajor decorations should not be applied to
    // them.
    if (auto *matType = dyn_cast<MatrixType>(fieldType)) {
      if (isa<FloatType>(matType->getElementType())) {
        memberAlignment = memberSize = stride = 0;
        std::tie(memberAlignment, memberSize) =
            getAlignmentAndSize(fieldType, rule, &stride);

        emitDecoration(typeResultId, spv::Decoration::MatrixStride, {stride},
                       index);

        // We need to swap the RowMajor and ColMajor decorations since HLSL
        // matrices are conceptually row-major while SPIR-V are conceptually
        // column-major.
        if (matType->isRowMajorMat()) {
          emitDecoration(typeResultId, spv::Decoration::ColMajor, {}, index);
        } else {
          // If the source code has neither row_major nor column_major
          // annotated, it should be treated as column_major since that's the
          // default.
          emitDecoration(typeResultId, spv::Decoration::RowMajor, {}, index);
        }
      }
    }

    ++index;
  }
}

void EmitTypeHandler::emitDecoration(uint32_t typeResultId,
                                     spv::Decoration decoration,
                                     llvm::ArrayRef<uint32_t> decorationParams,
                                     uint32_t memberIndex) {

  spv::Op op = memberIndex ? spv::Op::OpMemberDecorate : spv::Op::OpDecorate;
  assert(curDecorationInst.empty());
  curDecorationInst.push_back(static_cast<uint32_t>(op));
  curDecorationInst.push_back(typeResultId);
  if (memberIndex)
    curDecorationInst.push_back(memberIndex);
  curDecorationInst.push_back(static_cast<uint32_t>(decoration));
  for (auto param : decorationParams)
    curDecorationInst.push_back(param);
  curDecorationInst[0] |= static_cast<uint32_t>(curDecorationInst.size()) << 16;

  // Add to the full annotations list
  annotationsBinary->insert(annotationsBinary->end(), curDecorationInst.begin(),
                            curDecorationInst.end());
  curDecorationInst.clear();
}

} // end namespace spirv
} // end namespace clang
