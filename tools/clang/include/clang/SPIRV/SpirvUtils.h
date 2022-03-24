//===-- SpirvUtils.h - SPIR-V Utils ---------------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVUTILS_H
#define LLVM_CLANG_SPIRV_SPIRVUTILS_H

#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilSignatureElement.h"
#include "spirv/unified1/spirv.hpp11"
#include "clang/SPIRV/SpirvContext.h"

namespace clang {
namespace spirv {

class SpirvUtils {
public:
  SpirvUtils(SpirvContext &context);

  // Get SPIR-V execution model from HLSL shader model kind.
  static spv::ExecutionModel getSpirvShaderStage(hlsl::ShaderModel::Kind smk);

  // Apply required transformations to SPIR-V instruction's result type. These
  // transformations are needed by both the SPIR-V backend's LowerTypeVisitor as
  // well as the dxil2spv translator's SpirvTypeVisitor.
  void applyResultTypeTransformations(SpirvInstruction *instr);

private:
  SpirvContext &spvContext;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVUTILS_H
