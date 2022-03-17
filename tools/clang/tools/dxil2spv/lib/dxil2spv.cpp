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
  createEntryFunction(program.GetEntryFunction());

  // Contsruct the SPIR-V module.
  std::vector<uint32_t> m = spvBuilder.takeModuleForDxilToSpv();

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
        spvContext.getPointerType(toSpirvType(elem.get()),
                                  spv::StorageClass::Input),
        spv::StorageClass::Input, elem->GetSemanticName(), false, {});
    interfaceVars.push_back(var);
    inputSignatureElementMap[elem->GetID()] = var;
  }

  // Translate DXIL input signature to SPIR-V stage ouput vars.
  for (const std::unique_ptr<hlsl::DxilSignatureElement> &elem :
       outputSignature) {
    clang::spirv::SpirvVariable *var = spvBuilder.addStageIOVar(
        spvContext.getPointerType(toSpirvType(elem.get()),
                                  spv::StorageClass::Output),
        spv::StorageClass::Output, elem->GetSemanticName(), false, {});
    interfaceVars.push_back(var);
    outputSignatureElementMap[elem->GetID()] = var;
  }
}

void Translator::createEntryFunction(llvm::Function *entryFunction) {
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
    default: {
      emitError("Unhandled DXIL opcode");
    } break;
    }
  } else if (isa<llvm::ReturnInst>(instruction)) {
    spvBuilder.createReturn({});
  }
}

void Translator::createLoadInputInstruction(llvm::CallInst &instruction) {
  unsigned inputID =
      cast<llvm::ConstantInt>(instruction.getArgOperand(
                                  hlsl::DXIL::OperandIndex::kLoadInputIDOpIdx))
          ->getLimitedValue();
  spirv::SpirvVariable *inputVar = inputSignatureElementMap[inputID];
  const spirv::SpirvType *pointeeType =
      cast<spirv::SpirvPointerType>(inputVar->getResultType())
          ->getPointeeType();

  // TODO: Handle other input signature types. Only vector for initial
  // passthrough shader support.
  assert(isa<spirv::VectorType>(pointeeType));
  const spirv::SpirvType *elemType =
      cast<spirv::VectorType>(pointeeType)->getElementType();

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
  const spirv::SpirvType *pointeeType =
      cast<spirv::SpirvPointerType>(outputVar->getResultType())
          ->getPointeeType();

  // TODO: Handle other output signature types. Only vector for initial
  // passthrough shader support.
  assert(isa<spirv::VectorType>(pointeeType));
  const spirv::SpirvType *elemType =
      cast<spirv::VectorType>(pointeeType)->getElementType();

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

const spirv::SpirvType *Translator::toSpirvType(hlsl::CompType compType) {
  if (compType.IsFloatTy() || compType.IsSNorm() || compType.IsUNorm())
    return spvContext.getFloatType(compType.GetSizeInBits());
  else if (compType.IsSIntTy())
    return spvContext.getSIntType(compType.GetSizeInBits());
  else if (compType.IsUIntTy())
    return spvContext.getUIntType(compType.GetSizeInBits());

  llvm_unreachable("Unhandled DXIL Component Type");
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
  return toSpirvType(hlsl::CompType::GetCompType(llvmType));
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N]) {
  const auto diagId =
      diagnosticsEngine.getCustomDiagID(DiagnosticsEngine::Error, message);
  return diagnosticsEngine.Report({}, diagId);
}

} // namespace dxil2spv
} // namespace clang
