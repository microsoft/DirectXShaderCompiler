///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxil2spv.h                                                                //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxil2spv main function.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef __DXIL2SPV_DXIL2SPV__
#define __DXIL2SPV_DXIL2SPV__

#include "dxc/DXIL/DxilResource.h"
#include "dxc/DXIL/DxilSignature.h"
#include "dxc/Support/SPIRVOptions.h"
#include "dxc/dxcapi.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvContext.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace dxil2spv {

class Translator {
public:
  Translator(CompilerInstance &instance);
  int Run();

private:
  CompilerInstance &ci;
  DiagnosticsEngine &diagnosticsEngine;
  spirv::SpirvCodeGenOptions &spirvOptions;
  spirv::SpirvContext spvContext;
  spirv::FeatureManager featureManager;
  spirv::SpirvBuilder spvBuilder;

  // SPIR-V interface variables.
  std::vector<spirv::SpirvVariable *> interfaceVars;

  // Map from DXIL input signature element IDs to corresponding SPIR-V
  // variables.
  llvm::DenseMap<unsigned, spirv::SpirvVariable *> inputSignatureElementMap;
  llvm::DenseMap<unsigned, spirv::SpirvVariable *> outputSignatureElementMap;

  // Map from HLSL resource class and range ID to corresponding SPIR-V variable.
  llvm::DenseMap<std::pair<unsigned, unsigned>, spirv::SpirvVariable *>
      resourceMap;

  // Map from DXIL instructions (values) to SPIR-V instructions.
  llvm::DenseMap<llvm::Value *, spirv::SpirvInstruction *> instructionMap;

  // Get corresponding SPIR-V instruction for a given DXIL instruction, with
  // error checking.
  spirv::SpirvInstruction *getSpirvInstruction(llvm::Value *instruction);

  // Create corresponding SPIR-V constant for a given DXIL instruction, with
  // error checking.
  spirv::SpirvInstruction *createSpirvConstant(llvm::Constant *instruction);

  // Create SPIR-V stage IO variable from DXIL input and output signatures.
  void createStageIOVariables(
      const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>>
          &inputSignature,
      const std::vector<std::unique_ptr<hlsl::DxilSignatureElement>>
          &outputSignature);

  // Create SPIR-V module variables from DXIL resources.
  void createModuleVariables(
      const std::vector<std::unique_ptr<hlsl::DxilResource>> &resources);

  // Create SPIR-V entry function from DXIL function.
  spirv::SpirvFunction *createEntryFunction(llvm::Function *function);

  // Create SPIR-V basic block from DXIL basic block.
  void createBasicBlock(llvm::BasicBlock &basicBlock);

  // Create SPIR-V instruction(s) from DXIL instruction.
  void createInstruction(llvm::Instruction &instruction);
  void createLoadInputInstruction(llvm::CallInst &instruction);
  void createStoreOutputInstruction(llvm::CallInst &instruction);
  void createThreadIdInstruction(llvm::CallInst &instruction);
  void createBinaryOpInstruction(llvm::BinaryOperator &instruction);
  void createHandleInstruction(llvm::CallInst &instruction);
  void createBufferLoadInstruction(llvm::CallInst &instruction);
  void createBufferStoreInstruction(llvm::CallInst &instruction);
  void createExtractValueInstruction(llvm::ExtractValueInst &instruction);

  // SPIR-V Tools wrapper functions.
  bool spirvToolsValidate(std::vector<uint32_t> *mod, std::string *messages);

  // Translate HLSL/DXIL types to corresponding SPIR-V types.
  const spirv::SpirvType *toSpirvType(hlsl::CompType compType);
  const spirv::SpirvType *toSpirvType(hlsl::DxilSignatureElement *elem);
  const spirv::SpirvType *toSpirvType(llvm::Type *llvmType);
  const spirv::SpirvType *toSpirvType(llvm::StructType *structType);

  // TODO: These variables are used for a temporary hack to assign descriptor
  // set and binding numbers that works only for the most simple cases (always
  // use descriptor set 0, increment binding number for each resource). Further
  // work is needed to translate non-trivial shaders.
  unsigned nextDescriptorSet = 0;
  unsigned nextBindingNo = 0;

  // Helper diagnostic functions for emitting error messages.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]);
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              llvm::Value &instruction);
};

} // namespace dxil2spv
} // namespace clang

#endif // __DXIL2SPV_DXIL2SPV__
