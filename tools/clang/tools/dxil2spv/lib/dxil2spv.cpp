///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxil2spv.cpp                                                              //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxil2spv main function.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxil2spv.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/Global.h"

#include "clang/SPIRV/SpirvType.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

#include "dxc/DXIL/DxilOperations.h"
#include "spirv-tools/libspirv.hpp"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/SPIRV/SpirvUtils.h"

namespace clang {
namespace dxil2spv {

Translator::Translator(CompilerInstance &instance)
    : ci(instance), diagnosticsEngine(ci.getDiagnostics()),
      spirvOptions(ci.getCodeGenOpts().SpirvOptions),
      featureManager(diagnosticsEngine, spirvOptions),
      spvBuilder(spvContext, spirvOptions, featureManager) {}

int Translator::Run() {
  // Read input file to memory buffer.
  std::string filename = ci.getCodeGenOpts().MainFileName;
  auto errorOrInputFile = llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (!errorOrInputFile) {
    emitError("Error reading %0: %1")
        << filename << errorOrInputFile.getError().message();
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }
  std::unique_ptr<llvm::MemoryBuffer> memoryBuffer =
      std::move(errorOrInputFile.get());
  const char *blobContext = memoryBuffer->getBufferStart();
  unsigned blobSize = memoryBuffer->getBufferSize();

  // Parse LLVM module from bitcode.
  llvm::LLVMContext llvmContext;
  std::unique_ptr<llvm::Module> module;
  const hlsl::DxilContainerHeader *blobHeader =
      reinterpret_cast<const hlsl::DxilContainerHeader *>(blobContext);
  if (hlsl::IsValidDxilContainer(blobHeader,
                                 blobHeader->ContainerSizeInBytes)) {

    // Get DXIL program from container.
    const hlsl::DxilPartHeader *partHeader =
        hlsl::GetDxilPartByType(blobHeader, hlsl::DxilFourCC::DFCC_DXIL);
    IFTBOOL(partHeader != nullptr, DXC_E_MISSING_PART);
    const hlsl::DxilProgramHeader *programHeader =
        reinterpret_cast<const hlsl::DxilProgramHeader *>(
            GetDxilPartData(partHeader));

    // Parse DXIL program to module.
    if (IsValidDxilProgramHeader(programHeader, partHeader->PartSize)) {
      std::string diagStr; // Note: diagStr is not used by the callee.
      GetDxilProgramBitcode(programHeader, &blobContext, &blobSize);
      module = hlsl::dxilutil::LoadModuleFromBitcode(
          llvm::StringRef(blobContext, blobSize), llvmContext, diagStr);
    }

    if (module == nullptr) {
      emitError("Could not parse DXIL module from bitcode");
      return DXC_E_GENERAL_INTERNAL_ERROR;
    }
  }
  // Parse LLVM module from IR.
  else {
    llvm::SMDiagnostic err;
    module = parseIR(memoryBuffer->getMemBufferRef(), err, llvmContext);

    if (module == nullptr) {
      emitError("Could not parse DXIL module from IR: %0") << err.getMessage();
      return DXC_E_GENERAL_INTERNAL_ERROR;
    }
  }

  // Construct DXIL module.
  hlsl::DxilModule &program = module->GetOrCreateDxilModule();

  const hlsl::ShaderModel *shaderModel = program.GetShaderModel();
  if (shaderModel->GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("Unknown shader model: %0") << shaderModel->GetName();

  // Set shader model kind and HLSL major/minor version.
  spvContext.setCurrentShaderModelKind(shaderModel->GetKind());
  spvContext.setMajorVersion(shaderModel->GetMajor());
  spvContext.setMinorVersion(shaderModel->GetMinor());

  // Set default addressing and memory model for SPIR-V module.
  spvBuilder.setMemoryModel(spv::AddressingModel::Logical,
                            spv::MemoryModel::GLSL450);

  // Add stage variable interface.
  createStageIOVariables(program.GetInputSignature().GetElements(),
                         program.GetOutputSignature().GetElements());

  // Create entry function.
  spirv::SpirvFunction *entryFunction =
      createEntryFunction(program.GetEntryFunction());

  // Set execution mode if necessary.
  if (spvContext.isPS()) {
    spvBuilder.addExecutionMode(entryFunction,
                                spv::ExecutionMode::OriginUpperLeft, {}, {});
  }
  if (spvContext.isCS()) {
    spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::LocalSize,
                                {program.GetNumThreads(0),
                                 program.GetNumThreads(1),
                                 program.GetNumThreads(2)},
                                {});
  }

  // Add HLSL resources.
  createModuleVariables(program.GetSRVs());
  createModuleVariables(program.GetUAVs());

  // Contsruct the SPIR-V module.
  std::vector<uint32_t> m = spvBuilder.takeModuleForDxilToSpv();

  // Validate the generated SPIR-V code.
  std::string messages;
  if (!spirvToolsValidate(&m, &messages)) {
    emitError("Generated SPIR-V is invalid: %0") << messages;
    // return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  // Disassemble SPIR-V for output.
  std::string assembly;
  spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
  uint32_t spirvDisOpts = (SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
                           SPV_BINARY_TO_TEXT_OPTION_INDENT);

  if (!spirvTools.Disassemble(m, &assembly, spirvDisOpts)) {
    emitError("SPIR-V disassembly failed");
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  *ci.getOutStream() << assembly;

  return 0;
}

void Translator::createStageIOVariables(
    const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>>
        &inputSignature,
    const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>>
        &outputSignature) {
  interfaceVars.reserve(inputSignature.size() + outputSignature.size());

  // Translate DXIL input signature to SPIR-V stage input vars.
  for (const std::unique_ptr<hlsl::DxilSignatureElement> &elem :
       inputSignature) {
    clang::spirv::SpirvVariable *var = spvBuilder.addStageIOVar(
        toSpirvType(elem.get()), spv::StorageClass::Input,
        elem->GetSemanticName(), false, {});
    unsigned id = elem->GetID();
    interfaceVars.push_back(var);
    inputSignatureElementMap[id] = var;

    // Use unique DXIL signature element ID as SPIR-V Location.
    spvBuilder.decorateLocation(var, id);
  }

  // Translate DXIL input signature to SPIR-V stage ouput vars.
  for (const std::unique_ptr<hlsl::DxilSignatureElement> &elem :
       outputSignature) {
    clang::spirv::SpirvVariable *var = spvBuilder.addStageIOVar(
        toSpirvType(elem.get()), spv::StorageClass::Output,
        elem->GetSemanticName(), false, {});
    unsigned id = elem->GetID();
    interfaceVars.push_back(var);
    outputSignatureElementMap[id] = var;

    // Use unique DXIL signature element ID as SPIR-V Location.
    spvBuilder.decorateLocation(var, id);
  }
}

void Translator::createModuleVariables(
    const std::vector<std::unique_ptr<hlsl::DxilResource>> &resources) {
  for (const std::unique_ptr<hlsl::DxilResource> &resource : resources) {
    llvm::Type *hlslType = resource->GetHLSLType();
    assert(hlslType->isPointerTy());
    llvm::Type *pointeeType =
        cast<llvm::PointerType>(hlslType)->getPointerElementType();
    spvBuilder.addModuleVar(toSpirvType(pointeeType),
                            spv::StorageClass::Uniform, false);
  }
}

spirv::SpirvFunction *
Translator::createEntryFunction(llvm::Function *entryFunction) {
  spirv::SpirvFunction *spirvEntryFunction =
      spvBuilder.beginFunction(toSpirvType(entryFunction->getReturnType()), {},
                               entryFunction->getName());

  // Translate entry function parameters.
  llvm::SmallVector<const spirv::SpirvType *, 4> spirvParamTypes;
  for (llvm::Argument &argument : entryFunction->getArgumentList()) {
    spirvParamTypes.push_back(toSpirvType(argument.getType()));
  }
  spirvEntryFunction->setFunctionType(spvContext.getFunctionType(
      spirvEntryFunction->getReturnType(), spirvParamTypes));

  // Create entry function basic blocks.
  for (llvm::BasicBlock &basicBlock : *entryFunction) {
    createBasicBlock(basicBlock);
  }
  spvBuilder.endFunction();
  spvBuilder.addEntryPoint(
      spirv::SpirvUtils::getSpirvShaderStage(
          spvContext.getCurrentShaderModelKind()),
      spirvEntryFunction, spirvEntryFunction->getFunctionName(), interfaceVars);
  return spirvEntryFunction;
}

void Translator::createBasicBlock(llvm::BasicBlock &basicBlock) {
  spvBuilder.setInsertPoint(spvBuilder.createBasicBlock());

  // Translate each intruction in basic block.
  for (llvm::Instruction &instruction : basicBlock) {
    createInstruction(instruction);
  }
}

void Translator::createInstruction(llvm::Instruction &instruction) {
  // Handle Call instructions.
  if (isa<llvm::CallInst>(instruction)) {
    llvm::CallInst &callInstruction = cast<llvm::CallInst>(instruction);
    // Many shader operations are represented by call instructions that call
    // external functions, whose name is prefixed with dx.op. (i.e.
    // dx.op.loadInput, dx.op.storeOutput).
    hlsl::DXIL::OpCode dxilOpcode =
        hlsl::OP::GetDxilOpFuncCallInst(&instruction);
    switch (dxilOpcode) {
    case hlsl::DXIL::OpCode::LoadInput: {
      createLoadInputInstruction(callInstruction);
    } break;
    case hlsl::DXIL::OpCode::StoreOutput: {
      createStoreOutputInstruction(callInstruction);
    } break;
    case hlsl::DXIL::OpCode::ThreadId: {
      createThreadIdInstruction(callInstruction);
    } break;
    default: {
      emitError("Unhandled DXIL opcode: %0")
          << hlsl::OP::GetOpCodeName(dxilOpcode);
    } break;
    }
  }
  // Handle binary operator instructions.
  else if (isa<llvm::BinaryOperator>(instruction)) {
    llvm::BinaryOperator &binaryInstruction =
        cast<llvm::BinaryOperator>(instruction);
    createBinaryOpInstruction(binaryInstruction);
  }
  // Handle return instructions.
  else if (isa<llvm::ReturnInst>(instruction)) {
    spvBuilder.createReturn({});
  }
  // Unhandled instruction type.
  else {
    std::string instStr;
    llvm::raw_string_ostream os(instStr);
    instruction.print(os);
    emitError("Unhandled LLVM instruction: %0") << os.str();
  }
}

void Translator::createLoadInputInstruction(llvm::CallInst &instruction) {
  unsigned inputID =
      cast<llvm::ConstantInt>(instruction.getArgOperand(
                                  hlsl::DXIL::OperandIndex::kLoadInputIDOpIdx))
          ->getLimitedValue();
  spirv::SpirvVariable *inputVar = inputSignatureElementMap[inputID];
  const spirv::SpirvType *inputVarType = inputVar->getResultType();

  // TODO: Handle other input signature types. Only vector for initial
  // passthrough shader support.
  assert(isa<spirv::VectorType>(inputVarType));
  const spirv::SpirvType *elemType =
      cast<spirv::VectorType>(inputVarType)->getElementType();

  // Translate index into variable to SPIR-V constant.
  const llvm::APInt col = dyn_cast<llvm::Constant>(
                              instruction.getArgOperand(
                                  hlsl::DXIL::OperandIndex::kLoadInputColOpIdx))
                              ->getUniqueInteger();
  spirv::SpirvConstant *index =
      spvBuilder.getConstantInt(spvContext.getUIntType(32), col);

  // Create access chain and load.
  spirv::SpirvAccessChain *loadPtr =
      spvBuilder.createAccessChain(elemType, inputVar, {index}, {});
  spirv::SpirvLoad *loadInstr = spvBuilder.createLoad(elemType, loadPtr, {});

  instructionMap[&instruction] = loadInstr;
}

void Translator::createStoreOutputInstruction(llvm::CallInst &instruction) {
  unsigned outputID = cast<llvm::ConstantInt>(
                          instruction.getArgOperand(
                              hlsl::DXIL::OperandIndex::kStoreOutputIDOpIdx))
                          ->getLimitedValue();
  spirv::SpirvVariable *outputVar = outputSignatureElementMap[outputID];
  const spirv::SpirvType *outputVarType = outputVar->getResultType();

  // TODO: Handle other output signature types. Only vector for initial
  // passthrough shader support.
  assert(isa<spirv::VectorType>(outputVarType));
  const spirv::SpirvType *elemType =
      cast<spirv::VectorType>(outputVarType)->getElementType();

  // Translate index into variable to SPIR-V constant.
  const llvm::APInt col = dyn_cast<llvm::Constant>(
                              instruction.getArgOperand(
                                  hlsl::DXIL::OperandIndex::kLoadInputColOpIdx))
                              ->getUniqueInteger();
  spirv::SpirvConstant *index =
      spvBuilder.getConstantInt(spvContext.getUIntType(32), col);

  // Create access chain and store.
  spirv::SpirvAccessChain *outputVarPtr =
      spvBuilder.createAccessChain(elemType, outputVar, {index}, {});
  spirv::SpirvInstruction *valueToStore =
      instructionMap[instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx)];
  spvBuilder.createStore(outputVarPtr, valueToStore, {});
}

void Translator::createThreadIdInstruction(llvm::CallInst &instruction) {
  // DXIL ThreadId(component) corresponds to a single element of the SPIR-V
  // BuiltIn GlobalInvocationId, which must be a uint3.
  auto uint = spvContext.getUIntType(32);
  auto uint3 = spvContext.getVectorType(uint, 3);

  // Create SPIR-V BuiltIn and add it to interface variables.
  spirv::SpirvVariable *globalInvocationId = spvBuilder.addStageBuiltinVar(
      uint3, spv::StorageClass::Input, spv::BuiltIn::GlobalInvocationId, false,
      {});
  interfaceVars.emplace_back(globalInvocationId);

  // Translate DXIL component to SPIR-V index.
  const llvm::APInt component =
      dyn_cast<llvm::Constant>(
          instruction.getArgOperand(hlsl::DXIL::OperandIndex::kUnarySrc0OpIdx))
          ->getUniqueInteger();
  spirv::SpirvConstant *index = spvBuilder.getConstantInt(uint, component);

  // Create access chain and load.
  spirv::SpirvAccessChain *loadPtr =
      spvBuilder.createAccessChain(uint, globalInvocationId, {index}, {});
  spirv::SpirvLoad *loadInstr = spvBuilder.createLoad(uint, loadPtr, {});

  instructionMap[&instruction] = loadInstr;
}

void Translator::createBinaryOpInstruction(llvm::BinaryOperator &instruction) {
  llvm::Instruction::BinaryOps opcode = instruction.getOpcode();
  spirv::SpirvBinaryOp *result;

  switch (opcode) {
  // Shift left instruction.
  case llvm::Instruction::Shl: {
    // Value to be shifted.
    spirv::SpirvInstruction *val = instructionMap[instruction.getOperand(0)];
    if (!val) {
      std::string instStr;
      llvm::raw_string_ostream os(instStr);
      instruction.print(os);
      emitError("Could not find translation of instruction operand 0: %0")
          << os.str();
      return;
    }

    // Amount to shift by.
    const spirv::IntegerType *uint32 = spvContext.getUIntType(32);
    spirv::SpirvConstant *spvShift = spvBuilder.getConstantInt(
        uint32, dyn_cast<llvm::Constant>(instruction.getOperand(1))
                    ->getUniqueInteger());

    result = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical, uint32, val,
                                       spvShift, {});
  } break;
  default: {
    emitError("Unhandled LLVM binary opcode: %0") << opcode;
    return;
  }
  }

  instructionMap[&instruction] = result;
}

bool Translator::spirvToolsValidate(std::vector<uint32_t> *mod,
                                    std::string *messages) {
  spvtools::SpirvTools tools(featureManager.getTargetEnv());

  tools.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  spvtools::ValidatorOptions options;
  return tools.Validate(mod->data(), mod->size(), options);
}

const spirv::SpirvType *Translator::toSpirvType(hlsl::CompType compType) {
  if (compType.IsFloatTy() || compType.IsSNorm() || compType.IsUNorm())
    return spvContext.getFloatType(compType.GetSizeInBits());
  else if (compType.IsSIntTy())
    return spvContext.getSIntType(compType.GetSizeInBits());
  else if (compType.IsUIntTy())
    return spvContext.getUIntType(compType.GetSizeInBits());

  emitError("Unhandled DXIL Component Type: %0") << compType.GetName();
  return nullptr;
}

const spirv::SpirvType *
Translator::toSpirvType(hlsl::DxilSignatureElement *elem) {
  uint32_t rowCount = elem->GetRows();
  uint32_t colCount = elem->GetCols();
  const spirv::SpirvType *componentType = toSpirvType(elem->GetCompType());

  if (rowCount == 1 && colCount == 1)
    return componentType;

  const spirv::SpirvType *vecType =
      spvContext.getVectorType(componentType, colCount);

  if (rowCount == 1)
    return vecType;

  return spvContext.getMatrixType(vecType, rowCount);
}

const spirv::SpirvType *Translator::toSpirvType(llvm::Type *llvmType) {
  if (llvmType->isVoidTy())
    return new spirv::VoidType();

  if (llvmType->isIntegerTy() || llvmType->isFloatingPointTy())
    return toSpirvType(hlsl::CompType::GetCompType(llvmType));

  if (llvmType->isStructTy()) {
    return toSpirvType(cast<llvm::StructType>(llvmType));
  }

  std::string typeStr;
  llvm::raw_string_ostream os(typeStr);
  llvmType->print(os);
  emitError("Unhandled LLVM type: %0") << os.str();

  return nullptr;
}

const spirv::SpirvType *Translator::toSpirvType(llvm::StructType *structType) {
  // Remove prefix from struct name.
  llvm::StringRef prefix = "struct.";
  llvm::StringRef name = structType->getName();
  if (name.startswith(prefix))
    name = name.drop_front(prefix.size());

  // ByteAddressBuffer types.
  if (name == "ByteAddressBuffer") {
    return spvContext.getByteAddressBufferType(/*isRW*/ false);
  }

  // RWByteAddressBuffer types.
  if (name == "RWByteAddressBuffer") {
    return spvContext.getByteAddressBufferType(/*isRW*/ true);
  }

  // Other struct types.
  std::vector<spirv::StructType::FieldInfo> fields;
  fields.reserve(structType->getNumElements());
  for (llvm::Type *elemType : structType->elements()) {
    fields.emplace_back(toSpirvType(elemType));
  }
  return spvContext.getStructType(fields, name);
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N]) {
  const auto diagId =
      diagnosticsEngine.getCustomDiagID(DiagnosticsEngine::Error, message);
  return diagnosticsEngine.Report({}, diagId);
}

} // namespace dxil2spv
} // namespace clang
