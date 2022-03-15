//===--- SpirvTypeVisitor.cpp - AST type to SPIR-V type impl -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SpirvTypeVisitor.h"
#include "clang/SPIRV/SpirvFunction.h"

namespace clang {
namespace spirv {

bool SpirvTypeVisitor::visitInstruction(SpirvInstruction *instr) {
  // Instruction-specific type updates

  const auto *resultType = instr->getResultType();
  switch (instr->getopcode()) {
  // Variables and function parameters must have a pointer type.
  case spv::Op::OpFunctionParameter:
  case spv::Op::OpVariable: {
    if (auto *var = dyn_cast<SpirvVariable>(instr)) {
      auto vkImgFeatures = spvContext.getVkImageFeaturesForSpirvVariable(var);
      if (vkImgFeatures.format != spv::ImageFormat::Unknown) {
        if (const auto *imageType = dyn_cast<ImageType>(resultType)) {
          resultType = spvContext.getImageType(imageType, vkImgFeatures.format);
          instr->setResultType(resultType);
        }
      }
    }
    const SpirvType *pointerType =
        spvContext.getPointerType(resultType, instr->getStorageClass());
    instr->setResultType(pointerType);
    break;
  }
  // Access chains must have a pointer type. The storage class for the pointer
  // is the same as the storage class of the access base.
  case spv::Op::OpAccessChain: {
    const auto *pointerType = spvContext.getPointerType(
        resultType,
        cast<SpirvAccessChain>(instr)->getBase()->getStorageClass());
    instr->setResultType(pointerType);
    break;
  }
  default:
    break;
  }

  return true;
}

} // namespace spirv
} // namespace clang
