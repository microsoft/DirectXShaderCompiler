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

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/Global.h"

#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvType.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Casting.h"
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

void Translator::Run() {
  // Read input file to memory buffer.
  std::string filename = ci.getCodeGenOpts().MainFileName;
  auto errorOrInputFile = llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (!errorOrInputFile) {
    emitError("Error reading %0: %1")
        << filename << errorOrInputFile.getError().message();
    return;
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
      return;
    }
  }
  // Parse LLVM module from IR.
  else {
    llvm::SMDiagnostic err;
    module = parseIR(memoryBuffer->getMemBufferRef(), err, llvmContext);

    if (module == nullptr) {
      emitError("Could not parse DXIL module from IR: %0") << err.getMessage();
      return;
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

  // Add HLSL resources.
  createModuleVariables(program.GetSRVs());
  createModuleVariables(program.GetUAVs());

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

  // Don't attempt to emit SPIR-V module if errors were encountered in
  // translation.
  if (diagnosticsEngine.getClient()->getNumErrors() > 0) {
    return;
  }

  // Contsruct the SPIR-V module.
  std::vector<uint32_t> m = spvBuilder.takeModuleForDxilToSpv();

  // Validate the generated SPIR-V code.
  std::string messages;
  if (!spirvToolsValidate(&m, &messages)) {
    emitError("Generated SPIR-V is invalid: %0") << messages;
  }

  // Disassemble SPIR-V for output.
  std::string assembly;
  spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
  uint32_t spirvDisOpts = (SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
                           SPV_BINARY_TO_TEXT_OPTION_INDENT);

  if (!spirvTools.Disassemble(m, &assembly, spirvDisOpts)) {
    emitError("SPIR-V disassembly failed");
    return;
  }

  *ci.getOutStream() << assembly;
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
    createStageIOVariable(elem.get());
  }

  // Translate DXIL input signature to SPIR-V stage ouput vars.
  for (const std::unique_ptr<hlsl::DxilSignatureElement> &elem :
       outputSignature) {
    createStageIOVariable(elem.get());
  }
}

void Translator::createStageIOVariable(hlsl::DxilSignatureElement *elem) {
  const spirv::SpirvType *spirvType = toSpirvType(elem);
  if (!spirvType) {
    emitError("Failed to translate DXIL signature element to SPIR-V: %0")
        << elem->GetName();
    return;
  }
  spv::StorageClass storageClass =
      elem->IsInput() ? spv::StorageClass::Input : spv::StorageClass::Output;
  const unsigned id = elem->GetID();
  spirv::SpirvVariable *var = nullptr;
  switch (elem->GetKind()) {
  case hlsl::Semantic::Kind::Position: {
    var = spvBuilder.addStageBuiltinVar(spirvType, storageClass,
                                        spv::BuiltIn::Position, false, {});
    break;
  }
  default: {
    var = spvBuilder.addStageIOVar(spirvType, storageClass,
                                   elem->GetSemanticName(), false, {});

    // Use unique DXIL signature element ID as SPIR-V Location.
    spvBuilder.decorateLocation(var, id);
    break;
  }
  }
  interfaceVars.push_back(var);

  if (storageClass == spv::StorageClass::Input) {
    inputSignatureElementMap[id] = var;
  } else {
    outputSignatureElementMap[id] = var;
  }
}

void Translator::createModuleVariables(
    const std::vector<std::unique_ptr<hlsl::DxilResource>> &resources) {
  for (const std::unique_ptr<hlsl::DxilResource> &resource : resources) {
    llvm::Type *hlslType = resource->GetHLSLType();
    assert(hlslType->isPointerTy());
    llvm::Type *pointeeType =
        cast<llvm::PointerType>(hlslType)->getPointerElementType();
    const spirv::SpirvType *spirvType = toSpirvType(pointeeType);
    if (!spirvType) {
      emitError("Failed to translate DXIL resource to SPIR-V: %0")
          << resource->GetID();
      return;
    }
    spirv::SpirvVariable *moduleVar =
        spvBuilder.addModuleVar(spirvType, spv::StorageClass::Uniform, false);
    spvBuilder.decorateDSetBinding(moduleVar, nextDescriptorSet,
                                   nextBindingNo++);
    resourceMap[{static_cast<unsigned>(resource->GetClass()),
                 resource->GetID()}] = moduleVar;
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
      break;
    }
    case hlsl::DXIL::OpCode::StoreOutput: {
      createStoreOutputInstruction(callInstruction);
      break;
    }
    case hlsl::DXIL::OpCode::ThreadId: {
      createThreadIdInstruction(callInstruction);
      break;
    }
    case hlsl::DXIL::OpCode::CreateHandle: {
      createHandleInstruction(callInstruction);
      break;
    }
    case hlsl::DXIL::OpCode::BufferLoad: {
      createBufferLoadInstruction(callInstruction);
      break;
    }
    case hlsl::DXIL::OpCode::BufferStore: {
      createBufferStoreInstruction(callInstruction);
      break;
    }
    default: {
      emitError("Unhandled DXIL opcode: %0")
          << hlsl::OP::GetOpCodeName(dxilOpcode);
      break;
    }
    }
  }
  // Handle binary operator instructions.
  else if (isa<llvm::BinaryOperator>(instruction)) {
    llvm::BinaryOperator &binaryInstruction =
        cast<llvm::BinaryOperator>(instruction);
    createBinaryOpInstruction(binaryInstruction);
  }
  // Handle extractvalue instructions.
  else if (isa<llvm::ExtractValueInst>(instruction)) {
    llvm::ExtractValueInst &extractValueInstruction =
        cast<llvm::ExtractValueInst>(instruction);
    createExtractValueInstruction(extractValueInstruction);
  }
  // Handle return instructions.
  else if (isa<llvm::ReturnInst>(instruction)) {
    spvBuilder.createReturn({});
  }
  // Unhandled instruction type.
  else {
    emitError("Unhandled LLVM instruction: %0", instruction);
  }
}

void Translator::createLoadInputInstruction(llvm::CallInst &instruction) {
  unsigned inputID =
      cast<llvm::ConstantInt>(instruction.getArgOperand(
                                  hlsl::DXIL::OperandIndex::kLoadInputIDOpIdx))
          ->getLimitedValue();
  spirv::SpirvVariable *inputVar = inputSignatureElementMap[inputID];
  if (!inputVar) {
    emitError(
        "No matching SPIR-V input variable found for load instruction: %0",
        instruction);
    return;
  }
  const spirv::SpirvType *inputVarType = inputVar->getResultType();

  // TODO: Handle other input signature types. Only vector for initial
  // passthrough shader support.
  if (!isa<spirv::VectorType>(inputVarType)) {
    emitError("Input signature type not yet supported for load instruction: %0",
              instruction);
    return;
  }
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
  if (!outputVar) {
    emitError(
        "No matching SPIR-V output variable found for store output ID: %0")
        << outputID;
    return;
  }
  const spirv::SpirvType *outputVarType = outputVar->getResultType();

  // TODO: Handle other output signature types. Only vector for initial
  // passthrough shader support.
  if (!isa<spirv::VectorType>(outputVarType)) {
    emitError(
        "Output signature type not yet supported for store instruction: %0",
        instruction);
    return;
  }
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
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx));
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
    spirv::SpirvInstruction *val =
        getSpirvInstruction(instruction.getOperand(0));

    // Amount to shift by.
    const spirv::IntegerType *uint32 = spvContext.getUIntType(32);
    spirv::SpirvConstant *spvShift = spvBuilder.getConstantInt(
        uint32, dyn_cast<llvm::Constant>(instruction.getOperand(1))
                    ->getUniqueInteger());

    result = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical, uint32, val,
                                       spvShift, {});
    break;
  }
  default: {
    emitError("Unhandled LLVM binary opcode: %0") << opcode;
    return;
  }
  }

  instructionMap[&instruction] = result;
}

void Translator::createHandleInstruction(llvm::CallInst &instruction) {
  unsigned resourceClass =
      cast<llvm::ConstantInt>(
          instruction.getArgOperand(
              hlsl::DXIL::OperandIndex::kCreateHandleResClassOpIdx))
          ->getLimitedValue();
  unsigned resourceRangeId =
      cast<llvm::ConstantInt>(
          instruction.getArgOperand(
              hlsl::DXIL::OperandIndex::kCreateHandleResIDOpIdx))
          ->getLimitedValue();
  spirv::SpirvVariable *inputVar =
      resourceMap[{resourceClass, resourceRangeId}];
  if (!inputVar) {
    emitError("No resource found corresponding to handle: %0", instruction);
    return;
  }

  instructionMap[&instruction] = inputVar;
}

void Translator::createBufferLoadInstruction(llvm::CallInst &instruction) {
  // TODO: Extend this function to work with all buffer types on which it is
  // used, not just ByteAddressBuffers.

  // ByteAddressBuffers are represented as a struct with one member that is a
  // runtime array of unsigned integers. The SPIR-V OpAccessChain instruction is
  // then used to access that offset, and OpLoad is used to load integers
  // into a corresponding struct.

  // clang-format off
  // For example, the following DXIL instruction:
  //   %dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 } 
  //   %ret = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %res, i32 %index, i32 undef)

  // would translate to the following SPIR-V instructions:
  //   %dx_types_ResRet_i32 = OpTypeStruct %int %int %int %int %int
  //   %_ptr_Function_dx_types_ResRet_i32 = OpTypePointer Function %dx_types_ResRet_i32
  //   %ret = OpVariable %_ptr_Function_dx_types_ResRet_i32 Function
  //     %i = OpLoad %uint %index
  // for %offset = {0, 1, 2, 3, 4}, repeat:
  //    %v0 = OpIAdd %uint %i %offset
  //    %v1 = OpAccessChain %_ptr_Uniform_uint %res %offset %v0
  //    %v2 = OpLoad %uint %v1
  //    %v3 = OpAccessChain %_ptr_Function_int %ret %offset
  //    %v4 = OpBitcast %int %v2
  //          OpStore %v3 %v4
  // clang-format on

  // Get module input variable corresponding to given DXIL handle.
  spirv::SpirvInstruction *inputVar =
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferLoadHandleOpIdx));

  // Translate DXIL instruction return type (expected to be a struct of
  // integers) to a SPIR-V type.
  const spirv::SpirvType *returnType = toSpirvType(instruction.getType());
  assert(isa<spirv::StructType>(returnType));
  const spirv::StructType *structType = cast<spirv::StructType>(returnType);

  // Create a return variable to initialize with values loaded from the buffer.
  spirv::SpirvVariable *returnVar =
      spvBuilder.addFnVar(structType, {}, "", false, nullptr);

  // Translate indices into resource buffer to SPIR-V instructions.
  auto uint32 = spvContext.getUIntType(32);
  spirv::SpirvConstant *indexIntoStruct =
      spvBuilder.getConstantInt(uint32, llvm::APInt(32, 0));
  spirv::SpirvInstruction *baseArrayIndex =
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferLoadCoord0OpIdx));

  // TODO: Handle coordinate c1 if defined.
  if (!isa<llvm::UndefValue>(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferLoadCoord1OpIdx))) {
    emitError("BufferLoad not yet implemented to handle given coordinates: %0",
              instruction);
    return;
  }

  // Initialize each field in the struct.
  for (size_t i = 0; i < structType->getFields().size(); i++) {
    // Add offset for current field.
    spirv::SpirvConstant *fieldOffset =
        spvBuilder.getConstantInt(uint32, llvm::APInt(32, i));
    spirv::SpirvInstruction *indexIntoArray = spvBuilder.createBinaryOp(
        spv::Op::OpIAdd, uint32, baseArrayIndex, fieldOffset, {});

    // Create access chain and load.
    spirv::SpirvAccessChain *loadPtr = spvBuilder.createAccessChain(
        uint32, inputVar, {indexIntoStruct, indexIntoArray}, {});
    spirv::SpirvInstruction *loadInstr =
        spvBuilder.createLoad(uint32, loadPtr, {});

    // Create access chain and store.
    const spirv::SpirvType *fieldType = structType->getFields()[i].type;
    spirv::SpirvAccessChain *storePtr =
        spvBuilder.createAccessChain(fieldType, returnVar, fieldOffset, {});
    // LLVM types are signless, so type conversions are not 1-to-1. A bitcast on
    // the unsigned integer may be necessary before storing.
    spirv::SpirvInstruction *valToStore =
        fieldType == uint32 ? loadInstr
                            : spvBuilder.createUnaryOp(
                                  spv::Op::OpBitcast, fieldType, loadInstr, {});
    spvBuilder.createStore(storePtr, valToStore, {});
  }

  instructionMap[&instruction] = returnVar;
}

void Translator::createBufferStoreInstruction(llvm::CallInst &instruction) {
  // TODO: Extend this function to work with all buffer types on which it is
  // used, not just ByteAddressBuffers.

  // ByteAddressBuffers are represented as a struct with one member that is a
  // runtime array of unsigned integers. The SPIR-V OpAccessChain instruction is
  // then used to access that offset, and OpStore is used to store integers
  // into the array.

  // clang-format off
  // For example, the following DXIL instruction:
  //   call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %res, i32 %c0, i32 undef, i32 %v0, i32 undef, i32 undef, i32 undef, i8 1)
  // would translate to the following SPIR-V instructions:
  //   %x = OpAccessChain %_ptr_Uniform_uint %res %uint_0 %c0
  //   %y = OpBitcast %uint %v0
  //         OpStore %x %y
  // clang-format on

  // Get module output variable corresponding to given DXIL handle.
  spirv::SpirvInstruction *outputVar =
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferStoreHandleOpIdx));

  // Translate indices into resource buffer to SPIR-V instructions.
  auto uint32 = spvContext.getUIntType(32);
  spirv::SpirvConstant *indexIntoStruct =
      spvBuilder.getConstantInt(uint32, llvm::APInt(32, 0));
  spirv::SpirvInstruction *index =
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferStoreCoord0OpIdx));

  // TODO: Handle coordinate c1 if defined.
  if (!isa<llvm::UndefValue>(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferStoreCoord1OpIdx))) {
    emitError("BufferStore not yet implemented to handle given coordinates: %0",
              instruction);
    return;
  }

  // TODO: Handle other masks and values.
  if (cast<llvm::ConstantInt>(
          instruction.getArgOperand(
              hlsl::DXIL::OperandIndex::kBufferStoreMaskOpIdx))
          ->getLimitedValue() != 1) {
    emitError(
        "BufferStore not yet implemented to handle given mask and values: %0",
        instruction);
    return;
  }

  // Create access chain and store.
  spirv::SpirvAccessChain *outputVarPtr = spvBuilder.createAccessChain(
      uint32, outputVar, {indexIntoStruct, index}, {});
  spirv::SpirvInstruction *valueToStore =
      getSpirvInstruction(instruction.getArgOperand(
          hlsl::DXIL::OperandIndex::kBufferStoreVal0OpIdx));
  // LLVM types are signless, so type conversions are not 1-to-1. A bitcast on
  // the value may be necessary before storing.
  if (valueToStore->getResultType() != uint32)
    valueToStore =
        spvBuilder.createUnaryOp(spv::Op::OpBitcast, uint32, valueToStore, {});
  spvBuilder.createStore(outputVarPtr, valueToStore, {});
}

void Translator::createExtractValueInstruction(
    llvm::ExtractValueInst &instruction) {
  // Get corresponding SPIR-V intstruction for given aggregate value.
  spirv::SpirvInstruction *spvInstruction =
      getSpirvInstruction(instruction.getAggregateOperand());

  // Translate DXIL indices into aggregate to SPIR-V instructions.
  auto uint32 = spvContext.getUIntType(32);
  std::vector<spirv::SpirvInstruction *> indices;
  indices.reserve(instruction.getNumIndices());
  for (unsigned index : instruction.indices()) {
    spirv::SpirvConstant *spvIndex =
        spvBuilder.getConstantInt(uint32, llvm::APInt(32, index));
    indices.push_back(spvIndex);
  }

  // Create access chain and save mapping.
  const spirv::SpirvType *returnType = toSpirvType(instruction.getType());
  if (!returnType) {
    emitError("Failed to translate return type to SPIR-V for instruction: %0",
              instruction);
    return;
  }
  spirv::SpirvAccessChain *accessChain =
      spvBuilder.createAccessChain(returnType, spvInstruction, indices, {});

  instructionMap[&instruction] = accessChain;
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
    const spirv::SpirvType *spirvType = toSpirvType(elemType);
    if (!spirvType) {
      emitError("Failed to translate struct field to SPIR-V: %0", *elemType);
      return nullptr;
    }
    fields.emplace_back(spirvType);
  }
  return spvContext.getStructType(fields, name);
}

spirv::SpirvInstruction *
Translator::getSpirvInstruction(llvm::Value *instruction) {
  if (!instruction) {
    emitError("Unexpected null DXIL instruction");
    return nullptr;
  }

  if (spirv::SpirvInstruction *spirvInstruction = instructionMap[instruction])
    return spirvInstruction;

  if (auto *constant = llvm::dyn_cast<llvm::Constant>(instruction))
    return createSpirvConstant(constant);

  emitError("Expected SPIR-V instruction not found for DXIL instruction: %0",
            *instruction);
  return nullptr;
}

spirv::SpirvInstruction *
Translator::createSpirvConstant(llvm::Constant *instruction) {
  if (auto *fp = llvm::dyn_cast<llvm::ConstantFP>(instruction))
    return spvBuilder.getConstantFloat(spvContext.getFloatType(32),
                                       fp->getValueAPF());

  emitError("Unhandled LLVM constant instruction: %0", *instruction);
  return nullptr;
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N]) {
  const auto diagId =
      diagnosticsEngine.getCustomDiagID(DiagnosticsEngine::Error, message);
  return diagnosticsEngine.Report({}, diagId);
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N],
                                        llvm::Value &value) {
  std::string str;
  llvm::raw_string_ostream os(str);
  value.print(os);
  return emitError(message) << os.str();
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N],
                                        llvm::Type &type) {
  std::string str;
  llvm::raw_string_ostream os(str);
  type.print(os);
  return emitError(message) << os.str();
}

} // namespace dxil2spv
} // namespace clang
