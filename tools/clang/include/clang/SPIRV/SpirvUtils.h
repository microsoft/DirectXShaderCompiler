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

namespace clang {
namespace spirv {

class SpirvUtils {
public:
  static spv::ExecutionModel getSpirvShaderStage(hlsl::ShaderModel::Kind smk);
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVUTILS_H
