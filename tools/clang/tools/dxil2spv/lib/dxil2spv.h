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

  // Map from DXIL instructions (values) to SPIR-V instructions.
  llvm::DenseMap<llvm::Value *, spirv::SpirvInstruction *> instructionMap;

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

  // SPIR-V Tools wrapper functions.
  bool spirvToolsValidate(std::vector<uint32_t> *mod, std::string *messages);

  // Translate HLSL/DXIL types to corresponding SPIR-V types.
  const spirv::SpirvType *toSpirvType(hlsl::CompType compType);
  const spirv::SpirvType *toSpirvType(hlsl::DxilSignatureElement *elem);
  const spirv::SpirvType *toSpirvType(llvm::Type *llvmType);
  const spirv::SpirvType *toSpirvType(llvm::StructType *structType);

  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]);
};

} // namespace dxil2spv
} // namespace clang

#endif // __DXIL2SPV_DXIL2SPV__
