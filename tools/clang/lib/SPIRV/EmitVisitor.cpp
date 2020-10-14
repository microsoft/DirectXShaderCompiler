//===--- EmitVisitor.cpp - SPIR-V Emit Visitor Implementation ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "EmitVisitor.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/FileIOHelper.h"
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvType.h"
#include "clang/SPIRV/String.h"

namespace {

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

/// Returns true if an OpLine instruction can be emitted for the given OpCode.
/// According to the SPIR-V Spec section 2.4 (Logical Layout of a Module), the
/// first section to allow use of OpLine debug information is after all
/// annotation instructions.
bool isOpLineLegalForOp(spv::Op op) {
  switch (op) {
    // Preamble binary
  case spv::Op::OpCapability:
  case spv::Op::OpExtension:
  case spv::Op::OpExtInstImport:
  case spv::Op::OpMemoryModel:
  case spv::Op::OpEntryPoint:
  case spv::Op::OpExecutionMode:
  case spv::Op::OpExecutionModeId:
    // Debug binary
  case spv::Op::OpString:
  case spv::Op::OpSource:
  case spv::Op::OpSourceExtension:
  case spv::Op::OpSourceContinued:
  case spv::Op::OpName:
  case spv::Op::OpMemberName:
    // Annotation binary
  case spv::Op::OpModuleProcessed:
  case spv::Op::OpDecorate:
  case spv::Op::OpDecorateId:
  case spv::Op::OpMemberDecorate:
  case spv::Op::OpGroupDecorate:
  case spv::Op::OpGroupMemberDecorate:
  case spv::Op::OpDecorationGroup:
  case spv::Op::OpDecorateStringGOOGLE:
  case spv::Op::OpMemberDecorateStringGOOGLE:
    return false;
  default:
    return true;
  }
}

// Returns SPIR-V version that will be used in SPIR-V header section.
uint32_t getHeaderVersion(llvm::StringRef env) {
  if (env == "vulkan1.1")
    return 0x00010300u;
  if (env == "vulkan1.2")
    return 0x00010500u;
  return 0x00010000u;
}

// Returns true if the BufferBlock decoration is deprecated for the target
// Vulkan environment.
bool isBufferBlockDecorationDeprecated(
    const clang::spirv::SpirvCodeGenOptions &opts) {
  return opts.targetEnv.compare("vulkan1.2") >= 0;
}

// Read the file in |filePath| and returns its contents as a string.
// This function will be used by DebugSource to get its source code.
std::string ReadSourceCode(llvm::StringRef filePath) {
  try {
    dxc::DxcDllSupport dllSupport;
    IFT(dllSupport.Initialize());

    CComPtr<IDxcLibrary> pLibrary;
    IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));

    CComPtr<IDxcBlobEncoding> pSource;
    std::wstring srcFile(filePath.begin(), filePath.end());
    IFT(pLibrary->CreateBlobFromFile(srcFile.c_str(), nullptr, &pSource));

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFT(hlsl::DxcGetBlobAsUtf8(pSource, nullptr, &utf8Source));
    return std::string(utf8Source->GetStringPointer(),
                       utf8Source->GetStringLength());
  } catch (...) {
    // An exception has occured while reading the file
    return "";
  }
}

constexpr uint32_t kGeneratorNumber = 14;
constexpr uint32_t kToolVersion = 0;

} // anonymous namespace

namespace clang {
namespace spirv {

EmitVisitor::Header::Header(uint32_t bound_, uint32_t version_)
    // We are using the unfied header, which shows spv::Version as the newest
    // version. But we need to stick to 1.0 for Vulkan consumption by default.
    : magicNumber(spv::MagicNumber), version(version_),
      generator((kGeneratorNumber << 16) | kToolVersion), bound(bound_),
      reserved(0) {}

EmitVisitor::~EmitVisitor() {
  for (auto *i : spvInstructions)
    i->releaseMemory();
}

template <>
uint32_t
EmitVisitor::getOrAssignResultId<SpirvInstruction>(SpirvInstruction *obj) {
  auto *str = dyn_cast<SpirvString>(obj);
  if (str != nullptr) {
    auto it = stringIdMap.find(str->getString());
    if (it != stringIdMap.end()) {
      return it->second;
    }
  }

  if (!obj->getResultId()) {
    obj->setResultId(takeNextId());
  }
  if (str != nullptr) {
    stringIdMap[str->getString()] = obj->getResultId();
  }
  return obj->getResultId();
}

std::vector<uint32_t> EmitVisitor::Header::takeBinary() {
  std::vector<uint32_t> words;
  words.push_back(magicNumber);
  words.push_back(version);
  words.push_back(generator);
  words.push_back(bound);
  words.push_back(reserved);
  return words;
}

uint32_t EmitVisitor::getOrCreateOpStringId(llvm::StringRef str) {
  auto it = stringIdMap.find(str);
  if (it != stringIdMap.end()) {
    return it->second;
  }
  SpirvString *opString = new (context) SpirvString(/*SourceLocation*/ {}, str);
  visit(opString);
  spvInstructions.push_back(opString);
  return getOrAssignResultId<SpirvInstruction>(opString);
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
  debugVariableBinary.insert(debugVariableBinary.end(), curInst.begin(),
                             curInst.end());
}

void EmitVisitor::emitDebugLine(spv::Op op, const SourceLocation &loc,
                                std::vector<uint32_t> *section) {
  if (!spvOptions.debugInfoLine)
    return;

  // Technically entry function wrappers do not exist in HLSL. They
  // are just created by DXC. We do not want to emit line information
  // for their instructions.
  if (inEntryFunctionWrapper)
    return;

  // Based on SPIR-V spec, OpSelectionMerge must immediately precede either an
  // OpBranchConditional or OpSwitch instruction. Similarly OpLoopMerge must
  // immediately precede either an OpBranch or OpBranchConditional instruction.
  if (lastOpWasMergeInst) {
    lastOpWasMergeInst = false;
    return;
  }

  if (op == spv::Op::OpSelectionMerge || op == spv::Op::OpLoopMerge)
    lastOpWasMergeInst = true;

  if (!isOpLineLegalForOp(op))
    return;

  // DebugGlobalVariable and DebugLocalVariable of OpenCL.DebugInfo.100 already
  // has the line and the column information. We do not want to emit OpLine
  // for global variables and local variables. Instead, we want to emit OpLine
  // for their initialization if exists.
  if (op == spv::Op::OpVariable)
    return;

  auto fileId = debugMainFileId;
  const auto &sm = astContext.getSourceManager();
  const char *fileName = sm.getPresumedLoc(loc).getFilename();
  if (fileName)
    fileId = getOrCreateOpStringId(fileName);

  if (!fileId)
    return;

  uint32_t line = sm.getPresumedLineNumber(loc);
  uint32_t column = sm.getPresumedColumnNumber(loc);

  if (!line || !column)
    return;

  if (line == debugLine && column == debugColumn)
    return;

  assert(section);

  // We must update these two values to emit the next Opline.
  debugLine = line;
  debugColumn = column;

  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(spv::Op::OpLine));
  curInst.push_back(fileId);
  curInst.push_back(line);
  curInst.push_back(column);
  curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
  section->insert(section->end(), curInst.begin(), curInst.end());

  if (dumpedFiles.count(fileId) == 0) {
    SpirvString *fileNameInst =
        new (context) SpirvString(/*SourceLocation*/ {}, fileName);
    visit(fileNameInst);
    SpirvSource *src = new (context)
        SpirvSource(/*SourceLocation*/ {}, spv::SourceLanguage::HLSL,
                    hlslVersion, fileNameInst, "");
    visit(src);
    spvInstructions.push_back(fileNameInst);
    spvInstructions.push_back(src);
    dumpedFiles.insert(fileId);
  }
}

void EmitVisitor::initInstruction(SpirvInstruction *inst) {
  // Emit the result type if the instruction has a result type.
  if (inst->hasResultType()) {
    const uint32_t resultTypeId = typeHandler.emitType(inst->getResultType());
    inst->setResultTypeId(resultTypeId);
  }

  // Emit NonUniformEXT decoration (if any).
  if (inst->isNonUniform()) {
    typeHandler.emitDecoration(getOrAssignResultId<SpirvInstruction>(inst),
                               spv::Decoration::NonUniformEXT, {});
  }
  // Emit RelaxedPrecision decoration (if any).
  if (inst->isRelaxedPrecision()) {
    typeHandler.emitDecoration(getOrAssignResultId<SpirvInstruction>(inst),
                               spv::Decoration::RelaxedPrecision, {});
  }
  // Emit NoContraction decoration (if any).
  if (inst->isPrecise() && inst->isArithmeticInstruction()) {
    typeHandler.emitDecoration(getOrAssignResultId<SpirvInstruction>(inst),
                               spv::Decoration::NoContraction, {});
  }

  // According to Section 2.4. Logical Layout of a Module in the SPIR-V spec:
  // OpLine is always emitted to the main binary, except for global variables.
  // Global variables (variables whose storage class is NOT function) are
  // emitted before the main binary. They are allowed to have an OpLine
  // associated with them.
  bool isGlobalVar = false;
  if (auto *var = dyn_cast<SpirvVariable>(inst))
    isGlobalVar = var->getStorageClass() != spv::StorageClass::Function;
  const auto op = inst->getopcode();
  emitDebugLine(op, inst->getSourceLocation(),
                isGlobalVar ? &globalVarsBinary : &mainBinary);

  // Initialize the current instruction for emitting.
  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(op));
}

void EmitVisitor::initInstruction(spv::Op op, const SourceLocation &loc) {
  emitDebugLine(op, loc, &mainBinary);

  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(op));
}

void EmitVisitor::finalizeInstruction(std::vector<uint32_t> *section) {
  assert(section);
  curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
  section->insert(section->end(), curInst.begin(), curInst.end());
}

std::vector<uint32_t> EmitVisitor::takeBinary() {
  std::vector<uint32_t> result;
  Header header(takeNextId(), getHeaderVersion(spvOptions.targetEnv));
  auto headerBinary = header.takeBinary();
  result.insert(result.end(), headerBinary.begin(), headerBinary.end());
  result.insert(result.end(), preambleBinary.begin(), preambleBinary.end());
  result.insert(result.end(), debugFileBinary.begin(), debugFileBinary.end());
  result.insert(result.end(), debugVariableBinary.begin(),
                debugVariableBinary.end());
  result.insert(result.end(), annotationsBinary.begin(),
                annotationsBinary.end());
  result.insert(result.end(), typeConstantBinary.begin(),
                typeConstantBinary.end());
  result.insert(result.end(), globalVarsBinary.begin(), globalVarsBinary.end());
  result.insert(result.end(), richDebugInfo.begin(), richDebugInfo.end());
  result.insert(result.end(), mainBinary.begin(), mainBinary.end());
  return result;
}

void EmitVisitor::encodeString(llvm::StringRef value) {
  const auto &words = string::encodeSPIRVString(value);
  curInst.insert(curInst.end(), words.begin(), words.end());
}

bool EmitVisitor::visit(SpirvModule *, Phase) {
  // No pre-visit operations needed for SpirvModule.
  return true;
}

bool EmitVisitor::visit(SpirvFunction *fn, Phase phase) {
  assert(fn);

  // Before emitting the function
  if (phase == Visitor::Phase::Init) {
    const uint32_t returnTypeId = typeHandler.emitType(fn->getReturnType());
    const uint32_t functionTypeId = typeHandler.emitType(fn->getFunctionType());

    if (fn->isEntryFunctionWrapper())
      inEntryFunctionWrapper = true;

    // Emit OpFunction
    initInstruction(spv::Op::OpFunction, fn->getSourceLocation());
    curInst.push_back(returnTypeId);
    curInst.push_back(getOrAssignResultId<SpirvFunction>(fn));
    curInst.push_back(fn->isNoInline() ?
        static_cast<uint32_t>(spv::FunctionControlMask::DontInline) :
        static_cast<uint32_t>(spv::FunctionControlMask::MaskNone));
    curInst.push_back(functionTypeId);
    finalizeInstruction(&mainBinary);
    emitDebugNameForInstruction(getOrAssignResultId<SpirvFunction>(fn),
                                fn->getFunctionName());

    // RelaxedPrecision decoration may be applied to an OpFunction instruction.
    if (fn->isRelaxedPrecision())
      typeHandler.emitDecoration(getOrAssignResultId<SpirvFunction>(fn),
                                 spv::Decoration::RelaxedPrecision, {});
  }
  // After emitting the function
  else if (phase == Visitor::Phase::Done) {
    // Emit OpFunctionEnd
    initInstruction(spv::Op::OpFunctionEnd, /* SourceLocation */ {});
    finalizeInstruction(&mainBinary);
    inEntryFunctionWrapper = false;
  }

  return true;
}

bool EmitVisitor::visit(SpirvBasicBlock *bb, Phase phase) {
  assert(bb);

  // Before emitting the basic block.
  if (phase == Visitor::Phase::Init) {
    // Emit OpLabel
    initInstruction(spv::Op::OpLabel, /* SourceLocation */ {});
    curInst.push_back(getOrAssignResultId<SpirvBasicBlock>(bb));
    finalizeInstruction(&mainBinary);
    emitDebugNameForInstruction(getOrAssignResultId<SpirvBasicBlock>(bb),
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
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvExtension *ext) {
  initInstruction(ext);
  encodeString(ext->getExtensionName());
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvExtInstImport *inst) {
  initInstruction(inst);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  encodeString(inst->getExtendedInstSetName());
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvMemoryModel *inst) {
  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getAddressingModel()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryModel()));
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvEntryPoint *inst) {
  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getExecModel()));
  curInst.push_back(getOrAssignResultId<SpirvFunction>(inst->getEntryPoint()));
  encodeString(inst->getEntryPointName());
  for (auto *var : inst->getInterface())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(var));
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvExecutionMode *inst) {
  initInstruction(inst);
  curInst.push_back(getOrAssignResultId<SpirvFunction>(inst->getEntryPoint()));
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionMode()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction(&preambleBinary);
  return true;
}

bool EmitVisitor::visit(SpirvString *inst) {
  auto it = stringIdMap.find(inst->getString());
  if (it != stringIdMap.end())
    return true;
  uint32_t strId = getOrAssignResultId<SpirvInstruction>(inst);
  initInstruction(inst);
  curInst.push_back(strId);
  encodeString(inst->getString());
  finalizeInstruction(&debugFileBinary);
  stringIdMap[inst->getString()] = strId;
  return true;
}

bool EmitVisitor::visit(SpirvSource *inst) {
  // We should either emit OpSource or DebugSource, not both.
  // Therefore if rich debug info is being generated, we will skip
  // emitting OpSource.
  if (spvOptions.debugInfoRich)
    return true;

  // Emit the OpString for the file name.
  uint32_t fileId = debugMainFileId;
  if (inst->hasFile()) {
    fileId = getOrCreateOpStringId(inst->getFile()->getString());
    if (!debugMainFileId)
      debugMainFileId = fileId;
  }

  if (dumpedFiles.count(fileId) != 0)
    return true;
  dumpedFiles.insert(fileId);

  initInstruction(inst);
  curInst.push_back(static_cast<uint32_t>(inst->getSourceLanguage()));
  curInst.push_back(static_cast<uint32_t>(inst->getVersion()));
  if (hlslVersion == 0)
    hlslVersion = inst->getVersion();
  if (inst->hasFile())
    curInst.push_back(fileId);

  // Chop up the source into multiple segments if it is too long.
  llvm::Optional<llvm::StringRef> firstSnippet = llvm::None;
  llvm::SmallVector<llvm::StringRef, 2> choppedSrcCode;
  if (spvOptions.debugInfoSource && inst->hasFile()) {
    auto text = ReadSourceCode(inst->getFile()->getString());
    if (!text.empty()) {
      chopString(text, &choppedSrcCode);
      if (!choppedSrcCode.empty()) {
        firstSnippet = llvm::Optional<llvm::StringRef>(choppedSrcCode.front());
      }
    }

    if (firstSnippet.hasValue()) {
      // Note: in order to improve performance and avoid multiple copies, we
      // encode this (potentially large) string directly into the
      // debugFileBinary.
      const auto &words = string::encodeSPIRVString(firstSnippet.getValue());
      const auto numWordsInInstr = curInst.size() + words.size();
      curInst[0] |= static_cast<uint32_t>(numWordsInInstr) << 16;
      debugFileBinary.insert(debugFileBinary.end(), curInst.begin(),
                             curInst.end());
      debugFileBinary.insert(debugFileBinary.end(), words.begin(), words.end());
    } else {
      curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
      debugFileBinary.insert(debugFileBinary.end(), curInst.begin(),
                             curInst.end());
    }
  } else {
    curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
    debugFileBinary.insert(debugFileBinary.end(), curInst.begin(),
                           curInst.end());
  }

  // Now emit OpSourceContinued for the [second:last] snippet.
  for (uint32_t i = 1; i < choppedSrcCode.size(); ++i) {
    initInstruction(spv::Op::OpSourceContinued, /* SourceLocation */ {});
    // Note: in order to improve performance and avoid multiple copies, we
    // encode this (potentially large) string directly into the debugFileBinary.
    const auto &words = string::encodeSPIRVString(choppedSrcCode[i]);
    const auto numWordsInInstr = curInst.size() + words.size();
    curInst[0] |= static_cast<uint32_t>(numWordsInInstr) << 16;
    debugFileBinary.insert(debugFileBinary.end(), curInst.begin(),
                           curInst.end());
    debugFileBinary.insert(debugFileBinary.end(), words.begin(), words.end());
  }

  return true;
}

bool EmitVisitor::visit(SpirvModuleProcessed *inst) {
  initInstruction(inst);
  encodeString(inst->getProcess());
  finalizeInstruction(&annotationsBinary);
  return true;
}

bool EmitVisitor::visit(SpirvDecoration *inst) {
  initInstruction(inst);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getTarget()));
  if (inst->isMemberDecoration())
    curInst.push_back(inst->getMemberIndex());
  curInst.push_back(static_cast<uint32_t>(inst->getDecoration()));
  if (!inst->getParams().empty()) {
    curInst.insert(curInst.end(), inst->getParams().begin(),
                   inst->getParams().end());
  }
  if (!inst->getIdParams().empty()) {
    for (auto *paramInstr : inst->getIdParams())
      curInst.push_back(getOrAssignResultId<SpirvInstruction>(paramInstr));
  }
  finalizeInstruction(&annotationsBinary);
  return true;
}

bool EmitVisitor::visit(SpirvVariable *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getStorageClass()));
  if (inst->hasInitializer())
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getInitializer()));
  finalizeInstruction(inst->getStorageClass() == spv::StorageClass::Function
                          ? &mainBinary
                          : &globalVarsBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  if (spvOptions.enableReflect && inst->hasBinding() &&
      !inst->getHlslUserType().empty()) {
    typeHandler.emitDecoration(
        getOrAssignResultId<SpirvInstruction>(inst),
        spv::Decoration::UserTypeGOOGLE,
        string::encodeSPIRVString(inst->getHlslUserType().lower()));
  }
  return true;
}

bool EmitVisitor::visit(SpirvFunctionParameter *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoopMerge *inst) {
  initInstruction(inst);
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getMergeBlock()));
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getContinueTarget()));
  curInst.push_back(static_cast<uint32_t>(inst->getLoopControlMask()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvSelectionMerge *inst) {
  initInstruction(inst);
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getMergeBlock()));
  curInst.push_back(static_cast<uint32_t>(inst->getSelectionControlMask()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvBranch *inst) {
  initInstruction(inst);
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getTargetLabel()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvBranchConditional *inst) {
  initInstruction(inst);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getCondition()));
  curInst.push_back(getOrAssignResultId<SpirvBasicBlock>(inst->getTrueLabel()));
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getFalseLabel()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvKill *inst) {
  initInstruction(inst);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvReturn *inst) {
  initInstruction(inst);
  if (inst->hasReturnValue()) {
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getReturnValue()));
  }
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvSwitch *inst) {
  initInstruction(inst);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSelector()));
  curInst.push_back(
      getOrAssignResultId<SpirvBasicBlock>(inst->getDefaultLabel()));
  for (const auto &target : inst->getTargets()) {
    curInst.push_back(target.first);
    curInst.push_back(getOrAssignResultId<SpirvBasicBlock>(target.second));
  }
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvUnreachable *inst) {
  initInstruction(inst);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvAccessChain *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getBase()));
  for (const auto index : inst->getIndexes())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(index));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvAtomic *inst) {
  const auto op = inst->getopcode();
  initInstruction(inst);
  if (op != spv::Op::OpAtomicStore && op != spv::Op::OpAtomicFlagClear) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  }
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getPointer()));

  curInst.push_back(typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getScope())),
      context.getUIntType(32), /*isSpecConst */ false));

  curInst.push_back(typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getMemorySemantics())),
      context.getUIntType(32), /*isSpecConst */ false));

  if (inst->hasComparator())
    curInst.push_back(typeHandler.getOrCreateConstantInt(
        llvm::APInt(32,
                    static_cast<uint32_t>(inst->getMemorySemanticsUnequal())),
        context.getUIntType(32), /*isSpecConst */ false));

  if (inst->hasValue())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getValue()));
  if (inst->hasComparator())
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getComparator()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBarrier *inst) {
  const uint32_t executionScopeId =
      inst->isControlBarrier()
          ? typeHandler.getOrCreateConstantInt(
                llvm::APInt(32,
                            static_cast<uint32_t>(inst->getExecutionScope())),
                context.getUIntType(32), /*isSpecConst */ false)
          : 0;

  const uint32_t memoryScopeId = typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getMemoryScope())),
      context.getUIntType(32), /*isSpecConst */ false);

  const uint32_t memorySemanticsId = typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getMemorySemantics())),
      context.getUIntType(32), /* isSpecConst */ false);

  initInstruction(inst);
  if (inst->isControlBarrier())
    curInst.push_back(executionScopeId);
  curInst.push_back(memoryScopeId);
  curInst.push_back(memorySemanticsId);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand1()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand2()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldExtract *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getBase()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOffset()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getCount()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldInsert *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getBase()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getInsert()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOffset()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getCount()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantBoolean *inst) {
  typeHandler.getOrCreateConstant(inst);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantInteger *inst) {
  // Note: Since array types need to create uint 32-bit constants for result-id
  // of array length, the typeHandler keeps track of uint32 constant uniqueness.
  // Therefore emitting uint32 constants should be handled by the typeHandler.
  typeHandler.getOrCreateConstant(inst);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantFloat *inst) {
  typeHandler.getOrCreateConstant(inst);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantComposite *inst) {
  typeHandler.getOrCreateConstant(inst);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvConstantNull *inst) {
  typeHandler.getOrCreateConstant(inst);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeConstruct *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  for (const auto constituent : inst->getConstituents())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(constituent));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeExtract *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getComposite()));
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCompositeInsert *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getObject()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getComposite()));
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvEmitVertex *inst) {
  initInstruction(inst);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvEndPrimitive *inst) {
  initInstruction(inst);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvExtInst *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getInstruction());
  for (const auto operand : inst->getOperands())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(operand));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvFunctionCall *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvFunction>(inst->getFunction()));
  for (const auto arg : inst->getArgs())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(arg));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getExecutionScope())),
      context.getUIntType(32), /* isSpecConst */ false));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getArg1()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getArg2()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformElect *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getExecutionScope())),
      context.getUIntType(32), /* isSpecConst */ false));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, static_cast<uint32_t>(inst->getExecutionScope())),
      context.getUIntType(32), /* isSpecConst */ false));
  if (inst->hasGroupOp())
    curInst.push_back(static_cast<uint32_t>(inst->getGroupOp()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getArg()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageOp *inst) {
  initInstruction(inst);

  if (!inst->isImageWrite()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  }

  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getCoordinate()));

  if (inst->isImageWrite())
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getTexelToWrite()));

  if (inst->hasDref())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getDref()));
  if (inst->hasComponent())
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getComponent()));
  curInst.push_back(static_cast<uint32_t>(inst->getImageOperandsMask()));
  if (inst->getImageOperandsMask() != spv::ImageOperandsMask::MaskNone) {
    if (inst->hasBias())
      curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getBias()));
    if (inst->hasLod())
      curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getLod()));
    if (inst->hasGrad()) {
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getGradDx()));
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getGradDy()));
    }
    if (inst->hasConstOffset())
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getConstOffset()));
    if (inst->hasOffset())
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getOffset()));
    if (inst->hasConstOffsets())
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getConstOffsets()));
    if (inst->hasSample())
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getSample()));
    if (inst->hasMinLod())
      curInst.push_back(
          getOrAssignResultId<SpirvInstruction>(inst->getMinLod()));
  }
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageQuery *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getImage()));
  if (inst->hasCoordinate())
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getCoordinate()));
  if (inst->hasLod())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getLod()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageSparseTexelsResident *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getResidentCode()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvImageTexelPointer *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getCoordinate()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSample()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvLoad *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getPointer()));
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvCopyObject *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getPointer()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSampledImage *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getImage()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSampler()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSelect *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getCondition()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getTrueObject()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getFalseObject()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantBinaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand1()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand2()));
  finalizeInstruction(&typeConstantBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvStore *inst) {
  initInstruction(inst);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getPointer()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getObject()));
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvUnaryOp *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getOperand()));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvVectorShuffle *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getVec1()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getVec2()));
  for (const auto component : inst->getComponents())
    curInst.push_back(component);
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvArrayLength *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getStructure()));
  curInst.push_back(inst->getArrayMember());
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvRayTracingOpNV *inst) {
  initInstruction(inst);
  if (inst->hasResultType()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  }
  for (const auto operand : inst->getOperands())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(operand));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
                              inst->getDebugName());
  return true;
}

bool EmitVisitor::visit(SpirvDemoteToHelperInvocationEXT *inst) {
  initInstruction(inst);
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvDebugInfoNone *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugSource *inst) {
  uint32_t fileId = getOrCreateOpStringId(inst->getFile());
  if (!debugMainFileId)
    debugMainFileId = fileId;
  uint32_t textId = 0;
  if (spvOptions.debugInfoSource) {
    auto text = ReadSourceCode(inst->getFile());
    if (!text.empty())
      textId = getOrCreateOpStringId(text);
  }
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(fileId);
  if (textId)
    curInst.push_back(textId);
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugCompilationUnit *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(inst->getSpirvVersion());
  curInst.push_back(inst->getDwarfVersion());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugSource()));
  curInst.push_back(static_cast<uint32_t>(inst->getLanguage()));
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugLexicalBlock *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugScope *inst) {
  // Technically entry function wrappers do not exist in HLSL. They
  // are just created by DXC. We do not want to emit DebugScope for
  // it.
  if (inEntryFunctionWrapper)
    return true;

  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getScope()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvDebugFunctionDeclaration *inst) {
  uint32_t nameId = getOrCreateOpStringId(inst->getDebugName());
  uint32_t linkageNameId = getOrCreateOpStringId(inst->getLinkageName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(nameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(linkageNameId);
  curInst.push_back(inst->getFlags());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugFunction *inst) {
  uint32_t nameId = getOrCreateOpStringId(inst->getDebugName());
  uint32_t linkageNameId = getOrCreateOpStringId(inst->getLinkageName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(nameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(linkageNameId);
  curInst.push_back(inst->getFlags());
  curInst.push_back(inst->getScopeLine());
  auto *fn = inst->getSpirvFunction();
  if (fn) {
    curInst.push_back(getOrAssignResultId<SpirvFunction>(fn));
  } else {
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getDebugInfoNone()));
  }
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeBasic *inst) {
  uint32_t typeNameId = getOrCreateOpStringId(inst->getDebugName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(typeNameId);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSize()));
  curInst.push_back(inst->getEncoding());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeVector *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getElementType()));
  curInst.push_back(inst->getElementCount());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeArray *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getElementType()));

  // This is a reverse order of dimensions, thereby emitting in a reverse order.
  for (auto it = inst->getElementCount().rbegin();
       it != inst->getElementCount().rend(); ++it) {
    const auto countId = typeHandler.getOrCreateConstantInt(
        llvm::APInt(32, *it), context.getUIntType(32),
        /* isSpecConst */ false);
    curInst.push_back(countId);
  }

  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeFunction *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(inst->getDebugFlags());
  if (inst->getReturnType()) {
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getReturnType()));
  } else {
    // If return type is void, the return instruction must be OpTypeVoid.
    curInst.push_back(typeHandler.emitType(context.getVoidType()));
  }
  for (auto *paramType : inst->getParamTypes()) {
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(paramType));
  }
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeComposite *inst) {
  uint32_t typeNameId = getOrCreateOpStringId(inst->getDebugName());
  uint32_t linkageNameId = getOrCreateOpStringId(inst->getLinkageName());
  const auto size = typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, inst->getSizeInBits()), context.getUIntType(32),
      /* isSpecConst */ false);
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(typeNameId);
  curInst.push_back(inst->getTag());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(linkageNameId);
  if (inst->getDebugInfoNone()) {
    curInst.push_back(
        getOrAssignResultId<SpirvInstruction>(inst->getDebugInfoNone()));
  } else {
    curInst.push_back(size);
  }
  curInst.push_back(inst->getDebugFlags());
  for (auto *member : inst->getMembers()) {
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(member));
  }
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeMember *inst) {
  uint32_t typeNameId = getOrCreateOpStringId(inst->getDebugName());
  const auto offset = typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, inst->getOffsetInBits()), context.getUIntType(32),
      /* isSpecConst */ false);
  const auto size = typeHandler.getOrCreateConstantInt(
      llvm::APInt(32, inst->getSizeInBits()), context.getUIntType(32),
      /* isSpecConst */ false);
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(typeNameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(offset);
  curInst.push_back(size);
  curInst.push_back(inst->getDebugFlags());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeTemplate *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getTarget()));
  for (auto *param : inst->getParams()) {
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(param));
  }
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugTypeTemplateParameter *inst) {
  uint32_t typeNameId = getOrCreateOpStringId(inst->getDebugName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(typeNameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getActualType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getValue()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugLocalVariable *inst) {
  uint32_t nameId = getOrCreateOpStringId(inst->getDebugName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(nameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(inst->getFlags());
  if (inst->getArgNumber().hasValue())
    curInst.push_back(inst->getArgNumber().getValue());

  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugDeclare *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugLocalVariable()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getVariable()));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugExpression()));
  finalizeInstruction(&mainBinary);
  return true;
}

bool EmitVisitor::visit(SpirvDebugGlobalVariable *inst) {
  uint32_t nameId = getOrCreateOpStringId(inst->getDebugName());
  uint32_t linkageNameId = getOrCreateOpStringId(inst->getLinkageName());
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  curInst.push_back(nameId);
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getDebugType()));
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getSource()));
  curInst.push_back(inst->getLine());
  curInst.push_back(inst->getColumn());
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getParentScope()));
  curInst.push_back(linkageNameId);
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst->getVariable()));
  curInst.push_back(inst->getFlags());
  if (inst->getStaticMemberDebugDecl().hasValue())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(
        inst->getStaticMemberDebugDecl().getValue()));

  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvDebugExpression *inst) {
  initInstruction(inst);
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  curInst.push_back(
      getOrAssignResultId<SpirvInstruction>(inst->getInstructionSet()));
  curInst.push_back(inst->getDebugOpcode());
  for (const auto &op : inst->getOperations())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(op));
  finalizeInstruction(&richDebugInfo);
  return true;
}

bool EmitVisitor::visit(SpirvRayQueryOpKHR *inst) {
  initInstruction(inst);
  if (inst->hasResultType()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
  }
  for (const auto operand : inst->getOperands())
    curInst.push_back(getOrAssignResultId<SpirvInstruction>(operand));
  finalizeInstruction(&mainBinary);
  emitDebugNameForInstruction(getOrAssignResultId<SpirvInstruction>(inst),
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
                                             bool *alreadyExists) {
  assert(alreadyExists);
  auto foundType = emittedTypes.find(type);
  if (foundType != emittedTypes.end()) {
    *alreadyExists = true;
    return foundType->second;
  }

  *alreadyExists = false;
  const uint32_t id = takeNextIdFunction();
  emittedTypes[type] = id;
  return id;
}

uint32_t EmitTypeHandler::getOrCreateConstant(SpirvConstant *inst) {
  if (auto *constInt = dyn_cast<SpirvConstantInteger>(inst)) {
    return getOrCreateConstantInt(constInt->getValue(),
                                  constInt->getResultType(),
                                  inst->isSpecConstant(), inst);
  } else if (auto *constFloat = dyn_cast<SpirvConstantFloat>(inst)) {
    return getOrCreateConstantFloat(constFloat);
  } else if (auto *constComposite = dyn_cast<SpirvConstantComposite>(inst)) {
    return getOrCreateConstantComposite(constComposite);
  } else if (auto *constNull = dyn_cast<SpirvConstantNull>(inst)) {
    return getOrCreateConstantNull(constNull);
  } else if (auto *constBool = dyn_cast<SpirvConstantBoolean>(inst)) {
    return getOrCreateConstantBool(constBool);
  }

  llvm_unreachable("cannot emit unknown constant type");
}

uint32_t EmitTypeHandler::getOrCreateConstantBool(SpirvConstantBoolean *inst) {
  const auto index = static_cast<uint32_t>(inst->getValue());
  const bool isSpecConst = inst->isSpecConstant();

  // SpecConstants are not unique. We should not reuse them. e.g. it is possible
  // to have multiple OpSpecConstantTrue instructions.
  if (!isSpecConst && emittedConstantBools[index]) {
    // Already emitted this constant. Reuse.
    inst->setResultId(emittedConstantBools[index]->getResultId());
  } else {
    // Constant wasn't emitted in the past.
    const uint32_t typeId = emitType(inst->getResultType());
    initTypeInstruction(inst->getopcode());
    curTypeInst.push_back(typeId);
    curTypeInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
    finalizeTypeInstruction();
    // Remember this constant for the future (if not a spec constant)
    if (!isSpecConst)
      emittedConstantBools[index] = inst;
  }

  return inst->getResultId();
}

uint32_t EmitTypeHandler::getOrCreateConstantNull(SpirvConstantNull *inst) {
  auto found =
      std::find_if(emittedConstantNulls.begin(), emittedConstantNulls.end(),
                   [inst](SpirvConstantNull *cachedConstant) {
                     return *cachedConstant == *inst;
                   });

  if (found != emittedConstantNulls.end()) {
    // We have already emitted this constant. Reuse.
    inst->setResultId((*found)->getResultId());
  } else {
    // Constant wasn't emitted in the past.
    const uint32_t typeId = emitType(inst->getResultType());
    initTypeInstruction(spv::Op::OpConstantNull);
    curTypeInst.push_back(typeId);
    curTypeInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
    finalizeTypeInstruction();
    // Remember this constant for the future
    emittedConstantNulls.push_back(inst);
  }

  return inst->getResultId();
}

uint32_t EmitTypeHandler::getOrCreateConstantFloat(SpirvConstantFloat *inst) {
  llvm::APFloat value = inst->getValue();
  const SpirvType *type = inst->getResultType();
  const bool isSpecConst = inst->isSpecConstant();

  assert(isa<FloatType>(type));
  const auto *floatType = dyn_cast<FloatType>(type);
  const auto typeBitwidth = floatType->getBitwidth();
  const auto valueBitwidth = llvm::APFloat::getSizeInBits(value.getSemantics());
  auto valueToUse = value;

  // If the type and the value have different widths, we need to convert the
  // value to the width of the type. Error out if the conversion is lossy.
  if (valueBitwidth != typeBitwidth) {
    bool losesInfo = false;
    const llvm::fltSemantics &targetSemantics =
        typeBitwidth == 16 ? llvm::APFloat::IEEEhalf
                           : typeBitwidth == 32 ? llvm::APFloat::IEEEsingle
                                                : llvm::APFloat::IEEEdouble;
    const auto status = valueToUse.convert(
        targetSemantics, llvm::APFloat::roundingMode::rmTowardZero, &losesInfo);
    if (status != llvm::APFloat::opStatus::opOK &&
        status != llvm::APFloat::opStatus::opInexact) {
      emitError(
          "evaluating float literal %0 at a lower bitwidth loses information",
          {})
          // Converting from 16bit to 32/64-bit won't lose information.
          // So only 32/64-bit values can reach here.
          << std::to_string(valueBitwidth == 32 ? valueToUse.convertToFloat()
                                                : valueToUse.convertToDouble());
      return 0;
    }
  }

  auto valueTypePair = std::pair<uint64_t, const SpirvType *>(
      valueToUse.bitcastToAPInt().getZExtValue(), type);

  // SpecConstant instructions are not unique, so we should not re-use existing
  // spec constants.
  if (!isSpecConst) {
    // If this constant has already been emitted, return its result-id.
    auto foundResultId = emittedConstantFloats.find(valueTypePair);
    if (foundResultId != emittedConstantFloats.end()) {
      const uint32_t existingConstantResultId = foundResultId->second;
      inst->setResultId(existingConstantResultId);
      return existingConstantResultId;
    }
  }

  // Start constructing the instruction
  const uint32_t typeId = emitType(type);
  initTypeInstruction(inst->getopcode());
  curTypeInst.push_back(typeId);
  const uint32_t constantResultId = getOrAssignResultId<SpirvInstruction>(inst);
  curTypeInst.push_back(constantResultId);

  // Start constructing the value word / words

  if (typeBitwidth == 16) {
    // According to the SPIR-V Spec:
    // When the type's bit width is less than 32-bits, the literal's value
    // appears in the low-order bits of the word, and the high-order bits must
    // be 0 for a floating-point type.
    curTypeInst.push_back(
        static_cast<uint32_t>(valueToUse.bitcastToAPInt().getZExtValue()));
  } else if (typeBitwidth == 32) {
    curTypeInst.push_back(
        cast::BitwiseCast<uint32_t, float>(valueToUse.convertToFloat()));
  } else {
    // TODO: The ordering of the 2 words depends on the endian-ness of the
    // host machine.
    struct wideFloat {
      uint32_t word0;
      uint32_t word1;
    };
    wideFloat words =
        cast::BitwiseCast<wideFloat, double>(valueToUse.convertToDouble());
    curTypeInst.push_back(words.word0);
    curTypeInst.push_back(words.word1);
  }

  finalizeTypeInstruction();

  // Remember this constant for future (if not a SpecConstant)
  if (!isSpecConst)
    emittedConstantFloats[valueTypePair] = constantResultId;

  return constantResultId;
}

uint32_t
EmitTypeHandler::getOrCreateConstantInt(llvm::APInt value,
                                        const SpirvType *type, bool isSpecConst,
                                        SpirvInstruction *constantInstruction) {
  auto valueTypePair =
      std::pair<uint64_t, const SpirvType *>(value.getZExtValue(), type);

  // SpecConstant instructions are not unique, so we should not re-use existing
  // spec constants.
  if (!isSpecConst) {
    // If this constant has already been emitted, return its result-id.
    auto foundResultId = emittedConstantInts.find(valueTypePair);
    if (foundResultId != emittedConstantInts.end()) {
      const uint32_t existingConstantResultId = foundResultId->second;
      if (constantInstruction)
        constantInstruction->setResultId(existingConstantResultId);
      return existingConstantResultId;
    }
  }

  assert(isa<IntegerType>(type));
  const auto *intType = dyn_cast<IntegerType>(type);
  const auto bitwidth = intType->getBitwidth();
  const auto isSigned = intType->isSignedInt();

  // Start constructing the instruction
  const uint32_t typeId = emitType(type);
  initTypeInstruction(isSpecConst ? spv::Op::OpSpecConstant
                                  : spv::Op::OpConstant);
  curTypeInst.push_back(typeId);

  // Assign a result-id if one has not been provided.
  uint32_t constantResultId = 0;
  if (constantInstruction)
    constantResultId =
        getOrAssignResultId<SpirvInstruction>(constantInstruction);
  else
    constantResultId = takeNextIdFunction();

  curTypeInst.push_back(constantResultId);

  // Start constructing the value word / words

  // For 16-bit and 32-bit cases, the value occupies 1 word in the instruction
  if (bitwidth == 16 || bitwidth == 32) {
    if (isSigned) {
      curTypeInst.push_back(static_cast<int32_t>(value.getSExtValue()));
    } else {
      curTypeInst.push_back(static_cast<uint32_t>(value.getZExtValue()));
    }
  }
  // 64-bit cases
  else {
    struct wideInt {
      uint32_t word0;
      uint32_t word1;
    };
    wideInt words;
    if (isSigned) {
      words = cast::BitwiseCast<wideInt, int64_t>(value.getSExtValue());
    } else {
      words = cast::BitwiseCast<wideInt, uint64_t>(value.getZExtValue());
    }
    curTypeInst.push_back(words.word0);
    curTypeInst.push_back(words.word1);
  }

  finalizeTypeInstruction();

  // Remember this constant for future (not needed for SpecConstants)
  if (!isSpecConst)
    emittedConstantInts[valueTypePair] = constantResultId;

  return constantResultId;
}

uint32_t
EmitTypeHandler::getOrCreateConstantComposite(SpirvConstantComposite *inst) {
  // First make sure all constituents have been visited and have a result-id.
  for (auto constituent : inst->getConstituents())
    getOrCreateConstant(constituent);

  // SpecConstant instructions are not unique, so we should not re-use existing
  // spec constants.
  const bool isSpecConst = inst->isSpecConstant();
  SpirvConstantComposite **found = nullptr;

  if (!isSpecConst) {
    found = std::find_if(
        emittedConstantComposites.begin(), emittedConstantComposites.end(),
        [inst](SpirvConstantComposite *cachedConstant) {
          if (inst->getopcode() != cachedConstant->getopcode())
            return false;
          auto instConstituents = inst->getConstituents();
          auto cachedConstituents = cachedConstant->getConstituents();
          if (instConstituents.size() != cachedConstituents.size())
            return false;
          for (size_t i = 0; i < instConstituents.size(); ++i)
            if (instConstituents[i]->getResultId() !=
                cachedConstituents[i]->getResultId())
              return false;
          return true;
        });
  }

  if (!isSpecConst && found != emittedConstantComposites.end()) {
    // We have already emitted this constant. Reuse.
    inst->setResultId((*found)->getResultId());
  } else {
    // Constant wasn't emitted in the past.
    const uint32_t typeId = emitType(inst->getResultType());
    initTypeInstruction(spv::Op::OpConstantComposite);
    curTypeInst.push_back(typeId);
    curTypeInst.push_back(getOrAssignResultId<SpirvInstruction>(inst));
    for (auto constituent : inst->getConstituents())
      curTypeInst.push_back(getOrAssignResultId<SpirvInstruction>(constituent));
    finalizeTypeInstruction();

    // Remember this constant for the future (if not a spec constant)
    if (!isSpecConst)
      emittedConstantComposites.push_back(inst);
  }

  return inst->getResultId();
}

uint32_t EmitTypeHandler::emitType(const SpirvType *type) {
  // First get the decorations that would apply to this type.
  bool alreadyExists = false;
  const uint32_t id = getResultIdForType(type, &alreadyExists);

  // If the type has already been emitted, we just need to return its
  // <result-id>.
  if (alreadyExists)
    return id;

  // Emit OpName for the type (if any).
  emitNameForType(type->getName(), id);

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
    const uint32_t elementTypeId = emitType(vecType->getElementType());
    initTypeInstruction(spv::Op::OpTypeVector);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elementTypeId);
    curTypeInst.push_back(vecType->getElementCount());
    finalizeTypeInstruction();
  }
  // Matrix types
  else if (const auto *matType = dyn_cast<MatrixType>(type)) {
    const uint32_t vecTypeId = emitType(matType->getVecType());
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
    const uint32_t sampledTypeId = emitType(imageType->getSampledType());
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
    const uint32_t imageTypeId = emitType(sampledImageType->getImageType());
    initTypeInstruction(spv::Op::OpTypeSampledImage);
    curTypeInst.push_back(id);
    curTypeInst.push_back(imageTypeId);
    finalizeTypeInstruction();
  }
  // Array types
  else if (const auto *arrayType = dyn_cast<ArrayType>(type)) {
    // Emit the OpConstant instruction that is needed to get the result-id for
    // the array length.
    const auto length = getOrCreateConstantInt(
        llvm::APInt(32, arrayType->getElementCount()), context.getUIntType(32),
        /* isSpecConst */ false);

    // Emit the OpTypeArray instruction
    const uint32_t elemTypeId = emitType(arrayType->getElementType());
    initTypeInstruction(spv::Op::OpTypeArray);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elemTypeId);
    curTypeInst.push_back(length);
    finalizeTypeInstruction();

    auto stride = arrayType->getStride();
    if (stride.hasValue())
      emitDecoration(id, spv::Decoration::ArrayStride, {stride.getValue()});
  }
  // RuntimeArray types
  else if (const auto *raType = dyn_cast<RuntimeArrayType>(type)) {
    const uint32_t elemTypeId = emitType(raType->getElementType());
    initTypeInstruction(spv::Op::OpTypeRuntimeArray);
    curTypeInst.push_back(id);
    curTypeInst.push_back(elemTypeId);
    finalizeTypeInstruction();

    auto stride = raType->getStride();
    if (stride.hasValue())
      emitDecoration(id, spv::Decoration::ArrayStride, {stride.getValue()});
  }
  // Structure types
  else if (const auto *structType = dyn_cast<StructType>(type)) {
    llvm::ArrayRef<StructType::FieldInfo> fields = structType->getFields();
    size_t numFields = fields.size();

    // Emit OpMemberName for the struct members.
    for (size_t i = 0; i < numFields; ++i)
      emitNameForType(fields[i].name, id, i);

    llvm::SmallVector<uint32_t, 4> fieldTypeIds;
    for (auto &field : fields) {
      fieldTypeIds.push_back(emitType(field.type));
    }

    for (size_t i = 0; i < numFields; ++i) {
      auto &field = fields[i];
      // Offset decorations
      if (field.offset.hasValue())
        emitDecoration(id, spv::Decoration::Offset, {field.offset.getValue()},
                       i);

      // MatrixStride decorations
      if (field.matrixStride.hasValue())
        emitDecoration(id, spv::Decoration::MatrixStride,
                       {field.matrixStride.getValue()}, i);

      // RowMajor/ColMajor decorations
      if (field.isRowMajor.hasValue())
        emitDecoration(id,
                       field.isRowMajor.getValue() ? spv::Decoration::RowMajor
                                                   : spv::Decoration::ColMajor,
                       {}, i);

      // RelaxedPrecision decorations
      if (field.isRelaxedPrecision)
        emitDecoration(id, spv::Decoration::RelaxedPrecision, {}, i);

      // NonWritable decorations
      if (structType->isReadOnly())
        emitDecoration(id, spv::Decoration::NonWritable, {}, i);
    }

    // Emit Block or BufferBlock decorations if necessary.
    auto interfaceType = structType->getInterfaceType();
    if (interfaceType == StructInterfaceType::StorageBuffer)
      emitDecoration(id,
                     isBufferBlockDecorationDeprecated(spvOptions)
                         ? spv::Decoration::Block
                         : spv::Decoration::BufferBlock,
                     {});
    else if (interfaceType == StructInterfaceType::UniformBuffer)
      emitDecoration(id, spv::Decoration::Block, {});

    initTypeInstruction(spv::Op::OpTypeStruct);
    curTypeInst.push_back(id);
    for (auto fieldTypeId : fieldTypeIds)
      curTypeInst.push_back(fieldTypeId);
    finalizeTypeInstruction();
  }
  // Pointer types
  else if (const auto *ptrType = dyn_cast<SpirvPointerType>(type)) {
    const uint32_t pointeeType = emitType(ptrType->getPointeeType());
    initTypeInstruction(spv::Op::OpTypePointer);
    curTypeInst.push_back(id);
    curTypeInst.push_back(static_cast<uint32_t>(ptrType->getStorageClass()));
    curTypeInst.push_back(pointeeType);
    finalizeTypeInstruction();
  }
  // Function types
  else if (const auto *fnType = dyn_cast<FunctionType>(type)) {
    const uint32_t retTypeId = emitType(fnType->getReturnType());
    llvm::SmallVector<uint32_t, 4> paramTypeIds;
    for (auto *paramType : fnType->getParamTypes())
      paramTypeIds.push_back(emitType(paramType));

    initTypeInstruction(spv::Op::OpTypeFunction);
    curTypeInst.push_back(id);
    curTypeInst.push_back(retTypeId);
    for (auto paramTypeId : paramTypeIds)
      curTypeInst.push_back(paramTypeId);
    finalizeTypeInstruction();
  }
  // Acceleration Structure NV type
  else if (const auto *accType = dyn_cast<AccelerationStructureTypeNV>(type)) {
    initTypeInstruction(spv::Op::OpTypeAccelerationStructureNV);
    curTypeInst.push_back(id);
    finalizeTypeInstruction();
  }
  // RayQueryProvisionalType KHR type
  else if (const auto *rayQueryType =
               dyn_cast<RayQueryProvisionalTypeKHR>(type)) {
    initTypeInstruction(spv::Op::OpTypeRayQueryProvisionalKHR);
    curTypeInst.push_back(id);
    finalizeTypeInstruction();
  }
  // Hybrid Types
  // Note: The type lowering pass should lower all types to SpirvTypes.
  // Therefore, if we find a hybrid type when going through the emitting pass,
  // that is clearly a bug.
  else if (const auto *hybridType = dyn_cast<HybridType>(type)) {
    llvm_unreachable("found hybrid type when emitting SPIR-V");
  }
  // Unhandled types
  else {
    llvm_unreachable("unhandled type in emitType");
  }

  return id;
}

void EmitTypeHandler::emitDecoration(uint32_t typeResultId,
                                     spv::Decoration decoration,
                                     llvm::ArrayRef<uint32_t> decorationParams,
                                     llvm::Optional<uint32_t> memberIndex) {

  spv::Op op =
      memberIndex.hasValue() ? spv::Op::OpMemberDecorate : spv::Op::OpDecorate;
  if (decoration == spv::Decoration::UserTypeGOOGLE) {
    op = memberIndex.hasValue() ? spv::Op::OpMemberDecorateString
                                : spv::Op::OpDecorateString;
  }

  assert(curDecorationInst.empty());
  curDecorationInst.push_back(static_cast<uint32_t>(op));
  curDecorationInst.push_back(typeResultId);
  if (memberIndex.hasValue())
    curDecorationInst.push_back(memberIndex.getValue());
  curDecorationInst.push_back(static_cast<uint32_t>(decoration));
  for (auto param : decorationParams)
    curDecorationInst.push_back(param);
  curDecorationInst[0] |= static_cast<uint32_t>(curDecorationInst.size()) << 16;

  // Add to the full annotations list
  annotationsBinary->insert(annotationsBinary->end(), curDecorationInst.begin(),
                            curDecorationInst.end());
  curDecorationInst.clear();
}

void EmitTypeHandler::emitNameForType(llvm::StringRef name,
                                      uint32_t targetTypeId,
                                      llvm::Optional<uint32_t> memberIndex) {
  if (name.empty())
    return;
  std::vector<uint32_t> nameInstr;
  auto op = memberIndex.hasValue() ? spv::Op::OpMemberName : spv::Op::OpName;
  nameInstr.push_back(static_cast<uint32_t>(op));
  nameInstr.push_back(targetTypeId);
  if (memberIndex.hasValue())
    nameInstr.push_back(memberIndex.getValue());
  const auto &words = string::encodeSPIRVString(name);
  nameInstr.insert(nameInstr.end(), words.begin(), words.end());
  nameInstr[0] |= static_cast<uint32_t>(nameInstr.size()) << 16;
  debugVariableBinary->insert(debugVariableBinary->end(), nameInstr.begin(),
                              nameInstr.end());
}

} // end namespace spirv
} // end namespace clang
